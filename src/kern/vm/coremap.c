#include <types.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <addrspace.h>
#include <coremap.h>
#include <swapfile.h>
#include <vm.h>


struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
struct spinlock victim_lock = SPINLOCK_INITIALIZER;


//memory is seen as an array of coremap_entry (each one is a frame of 4096 B)
struct coremap_entry *coremap = NULL;

int num_ram_frames = 0;
int is_active = 0;

paddr_t victim = 0;
paddr_t last_allocate = 0;
int invalid_ref = 0;            //for allocation queue

/*
allocates and initialize the coremap, according to current ramsize value
*/
int coremap_init(){
    if (is_active){
        kprintf("coremap already active\n");
        return 1;
    }

    num_ram_frames = ((int)ram_getsize()) / PAGE_SIZE;
    coremap = kmalloc(sizeof(struct coremap_entry) * num_ram_frames);
    if (coremap==NULL)
        panic("failed to allocate coremap\n");
    
    for (int i=0; i<num_ram_frames; i++){
        coremap[i].type = UNTRACKED_ENTRY;
        coremap[i].alloc_size = 0;
        coremap[i].vaddr = 0;
        coremap[i].as = NULL;
        coremap[i].next_allocated = 0;
		coremap[i].prev_allocated = 0;
    }

    invalid_ref = num_ram_frames;
    last_allocate = invalid_ref;
    victim = invalid_ref;

    spinlock_acquire(&coremap_lock);
	is_active = 1;
	spinlock_release(&coremap_lock);

    return 0;
}


/*
deletes and frees entire coremap
*/
int coremap_close(){
    if (is_active==0)
        panic("Error, coremap not found\n");
    
    spinlock_acquire(&coremap_lock);
	is_active = 0;
	spinlock_release(&coremap_lock);

    kfree(coremap);

    return 0;
}


/*
return true if a coremap exists
*/
static int isCoremapActive(){
    int active;
    spinlock_acquire(&coremap_lock);
	active = is_active;
	spinlock_release(&coremap_lock);

    return active;
}


/*
searches for a contiguos interval of free frame in memory
if found returns the starting paddr
*/
static paddr_t search_free_pages(int npages){
    paddr_t addr = 0;
    int found = 0;

    for (int i=0; i<num_ram_frames; i++){
        if (coremap[i].type == UNTRACKED_ENTRY || coremap[i].type == FREED_ENTRY){
            found = i;
            for (int j=i; j<i+npages; j++){
                if (coremap[j].type != UNTRACKED_ENTRY && coremap[j].type != FREED_ENTRY){
                    found = 0;
                    break;
                }
            }
        }
        if (found != 0)
            break;
    }

    if (found == 0)
        return addr;
    
    //check if the value is inside boundaries
    if (found+npages > num_ram_frames)
        return addr;

    //get the physical address
    addr = (paddr_t)found * PAGE_SIZE;

    return addr;
}


/*
Memory management functions (replace dumbvm.c)
*/

/*
searches for a contiguos interval of freed frame in memory for kernel processes
if found occupies them and returns the starting paddr
*/
static paddr_t getfreeppages(unsigned long npages, int entry_type, struct addrspace *as, vaddr_t vadd){
    paddr_t addr = 0;
    int found = -1;
    int i,j;

    if (!isCoremapActive())
		return 0;

    spinlock_acquire(&coremap_lock);

    for (i=0; i<num_ram_frames; i++){
        if (coremap[i].type == FREED_ENTRY){
            found = i;
            for (j=i; j<i+npages; j++){
                if (coremap[j].type != FREED_ENTRY){
                    found = -1;
                    break;
                }
            }
        }
        if (found >=0)
            break;
    }

    if (found >= 0){
        for (i=found; i<found+npages; i++){
            coremap[i].type = entry_type;
            if (entry_type == USER_ENTRY){
                coremap[i].as = as;
                coremap[i].vaddr = vadd;
            }
            else if (entry_type == KERNEL_ENTRY){
                coremap[i].as = NULL;
                coremap[i].vaddr = 0;
            }
        }
        coremap[found].alloc_size = npages;
        addr = (paddr_t)found * PAGE_SIZE;
    }   
    
    spinlock_release(&coremap_lock);

    return addr;
}


/*
get n pages to occupy, for kernel processes
*/
static paddr_t getppages(unsigned long npages){
    paddr_t addr = 0;
    int i;

    //search in freed pages
    addr = getfreeppages(npages, KERNEL_ENTRY, NULL, 0);
    
    //if addr is still 0, pages not found
    if (addr == 0){
        addr = ram_stealmem(npages);

        if (addr != 0){
            spinlock_acquire(&stealmem_lock);
            int start_page = addr / PAGE_SIZE;
            coremap[start_page].alloc_size = npages;
            for (i=start_page; i<start_page+npages; i++){
                coremap[i].type = KERNEL_ENTRY;
            }
            spinlock_release(&stealmem_lock);
        }
    }

    //save the new pages in coremap array 
    /*if (addr != 0){
        int start_page = addr / PAGE_SIZE;

        spinlock_acquire(&stealmem_lock);
        coremap[start_page].alloc_size = npages;
        for (i=start_page; i<start_page+npages; i++){
            coremap[i].type = KERNEL_ENTRY;
        }
		spinlock_release(&stealmem_lock);
    }*/

    return addr;
}


/*
free the selectd number of pages, 
starting from the given physical address
*/
static int freeppages(paddr_t addr, unsigned long npages){
    int i;

    if (!isCoremapActive())
		return 0;
    
    int start_addr = addr / PAGE_SIZE;
    if (start_addr > num_ram_frames)
        panic("given address out of bounds\n");

    //set the page interval as freed
    spinlock_acquire(&coremap_lock);

    coremap[start_addr].alloc_size = 0;
    for (i=start_addr; i<start_addr+npages; i++){
        coremap[i].type = FREED_ENTRY;
        coremap[i].as = NULL;
        coremap[i].vaddr = 0;
    }

    spinlock_release(&coremap_lock);

    return 1;
}


/*
called by kmalloc(), allocates some *kernel* space
*/
vaddr_t alloc_kpages(unsigned npages){
    paddr_t padd;

    //suchvm_can_sleep();

    padd = getppages(npages);
    if (padd == 0)
        return 0;
    //return to virtual address for kernel
    return PADDR_TO_KVADDR(padd);
}


/*
called by kfree(), free kernel space, starting from given address
*/
void free_kpages(vaddr_t addr){
    if (!isCoremapActive())
        return;
    
    paddr_t padd;
    int start_add;
    //sub 2GB to convert into physical add
    padd = addr - MIPS_KSEG0;
    start_add = padd / PAGE_SIZE;
    //lenght of frames to free was set into first page
    freeppages(padd, coremap[start_add].alloc_size);
}



/*
User space management functions (with swap out/in)
*/

/*
allocates one page per time (on-demand) for user processes
*/
static paddr_t getppage_user(vaddr_t vadd){
    struct addrspace *as;
    // TODO: verificare con Edo i nomi delle variabili dei segmenti
    segment *victim_ps;
    paddr_t padd;
    paddr_t last_tmp, victim_tmp, victim_new;
    off_t offset;
    int res;

    //TODO: suchvm_can_sleep();

    as = proc_getas();
    if (as == NULL)
        panic("no address space found for this process\n");

    //alignment check
    KASSERT((vadd & PAGE_FRAME) == vadd);

    //first try to find a freed page
    padd = getfreeppages(1, USER_ENTRY, as, vadd);
    if (padd == 0){
        //not found, try ram_stealmem for one page
        spinlock_acquire(&stealmem_lock);
        padd = ram_stealmem(1);
        spinlock_release(&stealmem_lock);
    }

    //check if it is necessary to update the coremap
    if (isCoremapActive()){
        spinlock_acquire(&victim_lock);
		last_tmp = last_allocate;
		victim_tmp = victim;
		spinlock_release(&victim_lock);

        if (padd != 0){
            //found free space, update associate coremap entry
            spinlock_acquire(&coremap_lock);

            int found = padd / PAGE_SIZE;
            coremap[found].type = USER_ENTRY;
			coremap[found].alloc_size = 1;
			coremap[found].as = as;
			coremap[found].vaddr = vadd;
            if (last_tmp != invalid_ref){
                //page already allocated
				coremap[last_tmp].next_allocated = found;
				coremap[found].prev_allocated = last_tmp;
				coremap[found].next_allocated = invalid_ref;
			}
			else{
				coremap[found].prev_allocated = invalid_ref;
				coremap[found].next_allocated = invalid_ref;
			}
			
            spinlock_release(&coremap_lock);

            //update victim's info
            spinlock_acquire(&victim_lock);
			if (victim_tmp == invalid_ref)
				victim = found;
			last_allocate = found;
			spinlock_release(&victim_lock);
        }
        else {
            //memory is full, we have to swap out the victim page

            //physical address of victim 
            //it will be our freed page to be returned
            padd = (paddr_t)victim_tmp * PAGE_SIZE;

            //saves in offset the position in swapfile of victim
            res = swap_out(padd, &offset);
            if (res)
                panic("swap out failed\n");

            spinlock_acquire(&coremap_lock);
            victim_ps = as_find_segment(coremap[victim_tmp].as, coremap[victim_tmp].vaddr);
            //pt è in as, chiama pt_swap_out con quella pt, vadd e offset di swap
            lock_acquire(as->pt_lock);
            pt_swap_out(as->pt, coremap[victim_tmp].vaddr, offset);
            lock_release(as->pt_lock);

            //seg_swap_out(victim_ps, offset, coremap[victim_tmp].vaddr);

            //update coremap
            KASSERT(coremap[victim_tmp].type == USER_ENTRY);
			KASSERT(coremap[victim_tmp].alloc_size == 1);
            coremap[victim_tmp].vaddr = vadd;
			coremap[victim_tmp].as = as;

            victim_new = coremap[victim_tmp].next_allocated;
            coremap[last_tmp].next_allocated = victim_tmp;
			coremap[victim_tmp].next_allocated = invalid_ref;
			coremap[victim_tmp].prev_allocated = last_tmp;
			
            spinlock_release(&coremap_lock);

            //update victim's info
            spinlock_acquire(&victim_lock);
			last_allocate = victim_tmp;
            victim = victim_new;
			spinlock_release(&victim_lock);
        }
    }

    return padd;
}



/*
frees a previuos allocated user page
upgrades the linked list for victim's selection
*/
static void freeppage_user(paddr_t paddr){
    paddr_t new_last_all, new_victim;

    if (isCoremapActive()){
        //page to free and checks
        int found = paddr / PAGE_SIZE;
        KASSERT((int)num_ram_frames > found);
		KASSERT(coremap[found].alloc_size == 1);

        //save previous victim's info
        spinlock_acquire(&victim_lock);
		new_last_all = last_allocate;
		new_victim = victim;
		spinlock_release(&victim_lock);

        //update allocation queue
        spinlock_acquire(&coremap_lock);
        if (coremap[found].prev_allocated == invalid_ref){
            //first element in queue
            if (coremap[found].next_allocated == invalid_ref){
                //only element in queue
                new_victim = invalid_ref;
                new_last_all = invalid_ref;
            }
            else {
                //shift forward
                coremap[coremap[found].next_allocated].prev_allocated = invalid_ref;
                new_victim = coremap[found].next_allocated;
            }
        }
        else {
            //other element before it in queue
            if (coremap[found].next_allocated == invalid_ref){
                //last one --> shift backward
                coremap[coremap[found].prev_allocated].next_allocated = invalid_ref;
                new_last_all = coremap[found].prev_allocated;
            }
            else {
                //in the middle --> update both directions
                coremap[coremap[found].next_allocated].prev_allocated = coremap[found].prev_allocated;
				coremap[coremap[found].prev_allocated].next_allocated = coremap[found].next_allocated;
            }
        }
        coremap[found].next_allocated = invalid_ref;
        coremap[found].prev_allocated = invalid_ref;
        spinlock_release(&coremap_lock);

        //free the memory, using self-defined functions
        freeppages(paddr, 1);

        //update victim's info
        spinlock_acquire(&victim_lock);
		new_last_all = last_allocate;
		new_victim = victim;
		spinlock_release(&victim_lock);
    }
}
