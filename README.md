# Project c1: Virtual Memory with Demand Paging (PAGING)
Programmazione di Sistema A.Y. 2022/2023

- Edoardo Franco (<a href="https://github.com/ed0sh" target="_blank">@ed0sh</a>)
- Filippo Gorlero (<a href="https://github.com/gorle19" target="_blank">@gorle19</a>)
- Giulia Golzio (<a href="https://github.com/jelly1600" target="_blank">@jelly1600</a>)


## Introduction and Theoretical Info
- This is our implementation for **Project C1 (Paging)** of *pds_progetti_2023*. The provided set of files replaces the memory management defined in *dumbvm.c*, creating a new method based on on-demand pages requests, page tables and TLB usage, both with correct replacement and page swap-out/in algorithms
- The chosen variant of the project is **C1.1**, so we used per-process Page Tables; for the *empty regions* problem we used a linked list of segments
- **TLB** is unique inside our system and it is reserved for the current execuiting process, that is means the TLB must be invalidated at every context-swtiching

### Group management
We divided our work in 3 main areas: **TLB management**, **On-demand page loading** and **Page replacement**; each one of these was assigned to one of us who worked on his own. Every one or two days we did a online meeting where we updated the other members on our progresses and we worked together in parts of the code that had a connection between two or more different work areas. When the code parts was about to finish we started to create stats and test our code, with provided programs.

- For code sharing we used a GitHub repository, connected to the provided os161 Docker container 
- <a href="https://github.com/ed0sh/PoliTo-OS161-Project" target="_blank">Link to repository</a> 

### Virtual Memory and Paging
Virtual Memory is a method of abstracting the memory, with the aim of making it bigger and indipendent from the physical memory; it uses both RAM and storage space.
With Paging the physical memory is divided into "frames" and the virtual (or logical) memory into "pages": they have same dimension and each page can be related to one frame at a time. The mapping from logical to physical is made throgh structures named page tables.
Previously in os161 there was a basic virtual memory management module (`dumbvm`); in our project we will completely replace it with a more powerful one, based on Paging.

### Files and Implementation
Note: all of new files are defined in kern/vm, while the headers file in kern/include. The changes to already existing are inclosed in `#if OPT_PAGING`.

#### List of new files:
- coremap.c
- my_vm.c
- pt.c
- segments.c
- swapfile.c
- vm_tlb.c
- vmstats.c

#### List of modified files:
- addrspace.c
- runprogram.c
- loadelf.c


# TLB Management

## Data structure
In os/161 the Translation Lookaside Buffer is made of 64 entries, dimension defined by `NUM_TLB` in `tlb.h`. Each entry is composed by:
- a 20-bit virtual page number; 
- a 20-bit physical page number;
- some fields of which we will use only two: the validity bit and the dirty bit. The first is used to ensure that the virtual page is present in physical memory; the second is set if the page can be modified.

## Core concepts
Concerning the management of the TLB, we made use of some of the functions declared inside `tlb.h` (such as `tlb_probe`, `tlb_read` and `tlb_write`) and of some defined constants (in particular `NUM_TLB`, `TLBLO_VALID` and `TLBLO_DIRTY`).
We wrote in file `vm_tlb.c` the following functions, used to manage the TLB:
- `int tlb_get_rr_victim(void)`
- `void tlb_load(uint32_t entryhi, uint32_t entrylo, uint32_t perm)`
- `void tlb_invalidate(void)`
- `void tlb_invalidate_entry(vaddr_t vaddr)`

Concerning the VM management in general, we wrote the following functions inside file `my_vm.c`:
- `void vm_bootstrap(void)`
- `void vm_shutdown(void)`
- `void vm_tlbshootdown(const struct tlbshootdown *ts)`
- `void vm_can_sleep(void)`

### TLB Load New Entry
The function `tlb_get_rr_victim` implements a round-robin replacement policy and it returns an index indicating the next chosen victim:
```c
int tlb_get_rr_victim(void) {
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}
```

In `tlb_load` we managed the loading of a new entry inside the TLB:
- if there is unused space we place the new entry there; a slot is "unused" if the validity bit `TLBLO_VALID` isn't set;
- otherwise, if the table is full, we choose a new victim with the round-robin algorithm, depicted above.

We gave particular attention to the case when the virtul address passed to `vm_fault` and then to `tlb_load` is already present in an entry of the table: in this case we take as victim such entry, to surely avoid repeated virtual addresses inside the TLB.
```c
index = tlb_probe(entryhi, 0);
if (index < 0) {
    //vaddr not present in TLB
    for (i = 0; i < NUM_TLB; i++) {
        //looking for a free entry
        tlb_read(&v_hi, &p_lo, i);
        if (!(p_lo & TLBLO_VALID)) {
            victim=i;   
            break;
        }
    }
    if(victim == -1) {
        //no free entry, use round robin
        victim = tlb_get_rr_victim();
    } 
} else {
    //vaddr already present in TLB
    victim = index;
}
tlb_write(entryhi, entrylo, victim);
```

Moreover, through the parameter "perm" we discover if the new entry is writable and we take care of it setting the dirty bit `TLBLO_DIRTY`:
```c
if (perm & PF_W) {
    entrylo = entrylo | TLBLO_DIRTY;
}
```

### TLB Invalidation
The function `tlb_invalidate` is called by `as_activate` to invalidate all the TLB entries. In this way we ensure that after a context switch all TLB entries are "out-of-order": the new currently running process will be forced to load its own entries.
```c
// clear all the valid bits
for(i=0; i<NUM_TLB; i++) {
    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
}
```

Moreover, when a page is swapped out from the page table we invalidate the corresponding TLB entry calling `tlb_invalidate_entry`. This function searches in the TLB by virtual address and then invalidates the identified entry:
```c
// clear the valid bits
if((i = tlb_probe(vaddr, 0)) >= 0)
    tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
```

### TLB Fault
We handled TLB faults inside `my_vm.c`, with `vm_fault`. This function is called by the OS when a TLB miss occurs:
- it panics in case of VM_FAULT_READONLY;
- it returns EINVAL in case the parameter faulttype is incorrect; 
- it returns EFAULT in case something is wrong with the current process, the address space or the segment;
- on success, it loads a new entry inside the TLB calling `tlb_load` and then it returns 0.

The `vm_fault` is also partial responsible for the management of the page table. The page that made the `vm_fault` be called is examined:
- if it is a stack-page we set to zero the corresponding memory area;
- otherwise we analyse it through its `page_status`:
    - if `page_status` equal to `PT_ENTRY_EMPTY`, then the page isn't inizialized yet: we allocate some memory for it, we load it from the ELF program file and we add the new entry in page table;
    - if `page_status` equal to `PT_ENTRY_SWAPPED_OUT`, then the page was swapped-out: we retrive it from the swapfile, performing a swap-in;
    - if `page_status` equal to `PT_ENTRY_VALID`, then the page is valid: we just reload it inside the TLB.

```c
if (sg->base_vaddr == USERSTACK - sg->mem_size) {       // it's a stack page       
    [...]    
} else {
    // Management of the page inside the PT 
    [...]
    if (page_status == PT_ENTRY_EMPTY) {                // not-initialized (0)          
        [...]
    } else if (page_status == PT_ENTRY_SWAPPED_OUT) {   // swapped-out (1)
        [...]
    } else if (page_status == PT_ENTRY_VALID) {         // valid (2)
        // nothing to do
    }
}
// Management of the entry inside TLB 
tlb_load((uint32_t)aligned_faultaddress, (uint32_t)paddr, perm);

```


### VM Management
In order to correctly inizialized our virtual memory, we inizialized the swapfile structure and the virtual memory statistics:
```c
void vm_bootstrap(void) {
    swapfile_init();
    vmstats_init();
}
```

Concerning the correct shutdown of our virtual memory, we destroy the swapfile and then print our statistics:
```c
void vm_shutdown(void){
    swapfile_close();
    vmstats_print();
    vmstats_destroy();
}
```

In some case we have to check if we are in a context that can sleep and we call the funcion `vm_can_sleep`, declared inside `my_vm`.

# On-demand page loading
On-demand page loading refers to the feature of modern kernels where process data and code is loaded into physical as needed instead of pre-loading everything at start.<br>
To support this feature, we introduced/redefined in OS 161 structures like: **page table**, **segments** (code, data and stack) and **address space**.<br>
We also had to redefine how functions like `load_elf(...)` and `runprogram(...)` behaves.

## Page Table
### Data structure
We assumed a single-level per-process page table composed by a vector of `pt_entry` struct, each representing a page.
```c
/* kern/include/pt.h */

typedef struct _pagetable {
    uint32_t num_pages1;
    uint32_t num_pages2;
    vaddr_t start_vaddr1;
    vaddr_t start_vaddr2;
    pt_entry* pages;
} pagetable;
```
The total number of page is defined as the sum of the pages required by the program segments saved as `uint32_t num_pages1` and `uint32_t num_pages2`, while the virtual addresses used for the translation are `vaddr_t start_vaddr1, start_vaddr2`.<br>
We have two different start addresses in order to address the "hole" between the code and data segments virtual addresses (e.g. code start_vaddr = 0x400000, mem_size = 0x2000; data start_vaddr = 0x412000, mem_size = 0x2000).
```c
/* kern/include/pt.h */

typedef struct _pt_entry {
    uint8_t status;         
    paddr_t paddr;
    uint32_t perm;
    off_t swapfile_offset;
} pt_entry;
```
Each page table entry provide its curent status, assuming one of the following values:
```c
/* kern/include/pt.h */

#define PT_ENTRY_EMPTY 0
#define PT_ENTRY_SWAPPED_OUT 1
#define PT_ENTRY_VALID 2
```
It also carries information about the physical address (considered valid and returned only when `status == PT_ENTRY_VALID`); the read, write and execute permissions encoded as the activation of the three LSB, and - when `status == PT_ENTRY_VALID` - the offset in the swap file.

### Core concepts
#### Initialization and deallocation
The page table is initialized in the `as_prepare_load(...)`, once the address space and most importantly their segments has been initialized too.

The space is freed only once the process is no more running, indeed the page table deallocation function `pt_destroy(...)` is called inside `as_destroy(...)`;

#### Address transaltion
The main point of our page table implementation is the address translation algorithm: 
```c
/* kern/include/pt.c - pt_init(...) */
[...]

pt->start_vaddr1 = pt_start_vaddr1 & PAGE_FRAME;
pt->start_vaddr2 = pt_start_vaddr2 & PAGE_FRAME;
pt->pages = kmalloc((pt_num_pages1 + pt_num_pages2) * sizeof(pt_entry));

[...]

/* kern/include/pt.c */
[...]

vaddr_t aligned_vaddr = vaddr & PAGE_FRAME;
uint32_t pt_index;
if (aligned_vaddr >= pt->start_vaddr2)
    pt_index = ((aligned_vaddr - pt->start_vaddr2) / PAGE_SIZE) + pt->num_pages1;
else
    pt_index = (aligned_vaddr - pt->start_vaddr1) / PAGE_SIZE;

[...]
```
Each virtual address is first page-aligned, then compared with the page table `start_vaddr` and divided by `PAGE_SIZE` to get the index in the page vector. If the required address is in the "second" part of the array then a offset equal to the number of pages of the first part is added.<br>
Once there the path is straightforward.

## Program segments
The address space of a program contains a collection of segments that represents those read in the ELF file.<br>
In OS 161, the most important ones are: *data*, *code* and *stack*.

### Data structure
We decided to implement those segments as a linked list, in order to address the problem of the "empty" region between the stack and the other two segments.

```c
/* kern/include/segments.h */

typedef struct _segment {
    uint32_t perm;
    vaddr_t base_vaddr;
    off_t base_vaddr_offset;
    off_t file_offset;
    size_t file_size;
    size_t mem_size;
    size_t num_pages;
    struct _segment *next_segment;
} segment;
```

Each segment is composed by a set of properties that we briefly describe:
- `perm`: permissions that reflects exactly those described in the page table;
- `base_vaddr`: page-aligned segment virtual address;
- `base_vaddr_offset`: offset coming from the alignment of the virtual address, very important during the offset calculation needed to read a single page in the `load_page_from_elf(...)`;
- `file_offset`: the segment offset inside the ELF file;
- `file_size`: the segment size (in bytes) inside the ELF file;
- `mem_size`: the size of the data loaded into the memory;
- `next_segment`: pointer to the next segment.

### Core concepts
#### Initialization and deallocation
Segments are defined in the first - and now the only - complete read of the ELF file inside the `load_elf(...)`.

The `as_define_region(...)` is responsible for correctly setting the previously defined fields, but only for *data* and *code*.<br>
The stack segment instead is created by calling `as_define_stack(...)` from `runprogram(...)`.

The entire segment linked list is deallocated together with the process address space by calling `segments_destroy_linked_list(...)` inside `as_destroy(...)`.

## Address space
A basic implementation of the process address space would simply require the three program main segments, but other structures are needed to implement on-demand page loading.

### Data structure
We chose to entirely reinvent the previously specified `struct addrspace` starting from scratch.

```c
/* kern/include/addrspace.h  */

struct addrspace {
    segment *segments;
    struct vnode *v;
    pagetable *pt;
    size_t pt_num_pages;
    struct lock *pt_lock;
    char *progname;
};
```

As described in the [**Program segments**](README.md#program-segments) section, we defined the three program segments as a linked list, where the head of that list is `segment *segments`.

We've already talked about using a per-process page table. Here, we can see its usage as a `pagetable *pt`, coupled with its dimension `size_t pt_num_pages` and a lock `struct lock *pt_lock` that prevents operations to be performed on it simultaneously.

In order to load pages when needed, we also included a pointer to the program vnode `struct vnode *v`.

Finally, the program name `char *progname` is principally used while in `as_copy(...)`.

### Core concepts
#### Initialization and deallocation
The `struct addrspace` is initialized inside the function `as_create(...)`, whose main tasks are the struct allocation (using the kmalloc) and the creation of the lock `pt_lock`.

Deallocation happens inside `as_destroy(...)`, which: closes the open vnode by calling `vfs_close(as->v)`, destroys the page table `pt_destroy(as->pt)` and its lock `lock_destroy(as->pt_lock)`, deallocates the list of segments with `segments_destroy_linked_list(as->segments)` and finally frees the memory allocated for the struct itself with `kfree(as)`.

#### Page table initialization
The page table can't be initialized along with the address space, it needs the total number of pages that will compose its vector of pages.<br>
As described in the [**Program segments**](README.md#program-segments)  section, the `as_define_region(...)` appends to the address space segments the newly defined ones, along with the information acquired in each loop of the `load_elf(...)`.<br>
Once the `load_elf(...)` loop has ended and all the regions have been defined, the `as_prepare_load(...)` is then called. The core task of this function is to the define and initialize the page table.
```c
/* kern/vm/addrspace.c - as_prepare_load(...) */
[...]

for (segment *curseg = as->segments; curseg != NULL; curseg = curseg->next_segment){
    as->pt_num_pages += curseg->num_pages;
}

// Define page table
lock_acquire(as->pt_lock);

segment *curseg = as->segments;
vaddr_t base_vaddr1 = curseg->base_vaddr;
vaddr_t base_vaddr2 = 0;
size_t num_pages1 = curseg->num_pages;
size_t numpages_2 = 0;

while ((curseg = curseg->next_segment) != NULL) {
    if (curseg->base_vaddr < base_vaddr1) {
        base_vaddr2 = base_vaddr1;
        numpages_2 = num_pages1;
        base_vaddr1 = curseg->base_vaddr;
        num_pages1 = curseg->num_pages;
    } else {
        base_vaddr2 = curseg->base_vaddr;
        numpages_2 = curseg->num_pages;
    }
}

as->pt = pt_init(base_vaddr, as->pt_num_pages);

lock_release(as->pt_lock);

[...]
```

#### Stack definition
The stack segment is treated differently from the other two segments, it has pre-defined permissions, virtual address, sizes and offsets.<br>
Its definition is performed in the `as_define_stack(...)`, which is called by the `runprogram(...)` once the `load_elf(...)` is correctly terminated.

```c
/* kern/include/addrspace.h */
[...]
#define STACK_PAGES 18
[...]

/* kern/vm/addrspace.c - as_define_stack(...) */
[...]

size_t stack_size = STACK_PAGES * PAGE_SIZE;
if (as_define_region(as, USERSTACK - stack_size, stack_size, (PF_W | PF_R), 0, 0) != 0)
    return ENOMEM;

*stackptr = USERSTACK;

[...]
```

#### Runprogram
The `runprogram(...)` function is the one responsible for defining the address space and initializing it with the data read from the ELF file.<br>
The main changes we made are: the relocation of the `vfs_open(...)` from this function to the `as_create(...)` and the relocation od the `vfs_close(...)` from this function to the `as_destroy(...)`.

#### Load ELF
The `load_elf(...)` function no longer loads the program code and data into memory, but instead just defines the program segments inside the address space..

We wrote a new function to read data from the elf file and load *maximum* one page at a time into memory. <br>
Its signature is `load_page_from_elf(segment *seg, vaddr_t vaddr, paddr_t paddr)` and it's called only after a tlb fault, where the page is not in the page table nor in the swap space, by the `vm_fault(...)`.<br>
The main task of the `load_elf(...)` is the computation of the arguments needed by the `load_segment(...)` (which also underwent minor changes). 
Those arguments are:
* the target virtual address (where the data will be loaded);
* the offset (the location to begin reading from) inside the elf file;
* the file size (the amount of data to be read) and the memory size (the amount of data that will be loaded into memory).

For more detail see [`kern/syscall/loadelf.c`](./src/kern/syscall/loadelf.c#L363).


# Page replacement
Page replacement operations start when a Page Fault occours and there is no free space in memory. This set of functions implements a global replacement policy, with victim selection based on FIFO.

## Swapfile
The file `SWAPFILE` is a temporary parking for memory pages that we had to swap-out from memory, when there is no free space; the file dimension `SWAP_SIZE` (9 MB) is defined in `swapfile.h`. <br> 
In order to maintain a correspondence between memory and swapfile the file is divided in pages of `PAGE_SIZE` bytes, these pages are managed by a bitmap in order to trace the free/occupied entries. <br> 
In case of SWAPFILE is full a panic `out of swap space` is raised

### Data structures
File path: `kern/vm/swapfile.c`
```c
#define SWAP_PATH "emu0:/SWAPFILE"

struct vnode *swapfile;
struct bitmap *swapfile_map;
```
### Core concepts
After the bootstrap a void SWAPFILE with dimension `SWAP_SIZE` is created and saved in `SWAP_PATH`, that is because at the start of our system there are no swapped-out pages (so it isn't a classic backing store). A bitmap of `SWAP_SIZE/PAGE_SIZE` entries is used to trace the page's status in the SWAPFILE; using this variable when needed we don't have to remove physically a page from the file but only to set the corresponding bitmap entry as free (please note that all the bitmap management functions are the ones already provided in OS161 system).<br>
Using this method when a page is the selected victim and we need to remove it from memory we can write easily it in the first free page of SWAPFILE and mark it as swapped page; the found page index is saved within the address space of the process, in order to find it faster in case of the page must returns to memory (swap-in operation), with direct access to SWAPFILE page.<br>
The SWAPFILE entries are freed when a swap-in operation is requested, when the corresponding process terminates and during the shutdown
```
Only pages of a user process can be swapped-out from memory.
```
Access to SWAPFILE is protected using a spinlock to guarantee mutual exclusion. 

#### Swap-out
Swap-out operation is performed when a request of page allocation in memory for a user process cannot be completed due to no free space; after victim's selection (which physical address is the parameter `paddr`), the corresponding page is removed from memory and PT and written in the SWAPFILE, after saving the index of the found entry in `swap_offset`.
```c
int swap_out(paddr_t paddr, off_t *swap_offset){
    [...]
    res = bitmap_alloc(swapfile_map, &index);
        if (res)
            panic("Out of swap space\n");

    offset = index * PAGE_SIZE; 

    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, offset, UIO_WRITE);
        VOP_WRITE(swapfile, &u);

    *swap_offset = offset;
    [...]
}
```

#### Swap-in
The opposite operaton is performed when a requested page, that causes a Page Fault, is marked as swapped, which means it has the field `off_t swapfile_offset` in its PT entry different from NULL. In this case we don't have to search it in secondary memory but directly in SWAPFILE, using direct access via the given offset.<br>
In this function we only try to read the corresponding address in SWAPFILE and in case of success mark the bitmap's index as free.
```c
int swap_in(paddr_t paddr, off_t swap_offset){
    [...]
    index = swap_offset / PAGE_SIZE;

    if (!bitmap_isset(swapfile_map, index)) 
            panic("No swapped pages found at this address\n");

    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, swap_offset, UIO_READ);
        VOP_READ(swapfile, &u);
    [...]
    bitmap_unmark(swapfile_map, index);
    [...]
}
```

## Coremap
The `coremap` is a virtual replacement of RAM memory, that now is seen as an array of `coremap_entry` with dimension equal to the number of RAM's physical frame. <br>
We've done this because in this situation we are able to manage the memory easier and we can choose what we want to save inside our memory frames.

### Data structures
File path: `kern/vm/coremap.c`
```c
/* kern/include/coremap.h */
struct coremap_entry {
    int type;
    int alloc_size;
    vaddr_t vaddr;
    struct addrspace *as;
    //only for user space
    paddr_t prev_allocated, next_allocated;
};
```
```c
struct coremap_entry *coremap = NULL;
```
A single `coremap_entry` represents one pyhsical frame in memory; within its we found:
- allocation `type` (see later, we will discuss this afterwards); `alloc_size` is used to indicate the first frame in a contiguos allocation with the dimension in frame of that structure;
- `vaddr` is the virtual address of the frame; 
- `as` points the address space linked to the frame (only for user processes);
- `prev_allocated` and `next_allocated` create a linked list of frame used for replacement policy (only for user processes).

### Core concepts
This set of functions replaces completely the memory management functions defined in `dumbvm.c` and in `kamlloc.c`, in order to manage this using the new defined coremap.<br>
In addition there are a couple of particular functions to manage the user-side memory allocation, that is the one it can be interested from swapping operations.<br>
The re-defined functions are the following:

```c 
static paddr_t getfreeppages(unsigned long npages, int entry_type, struct addrspace *as, vaddr_t vadd)
static paddr_t getppages(unsigned long npages)
static int freeppages(paddr_t addr, unsigned long npages)
vaddr_t alloc_kpages(unsigned npages)
void free_kpages(vaddr_t addr)
```
Instead the new defined function for user processes are these: (note that unlike the previuos ones user processes can only request/free one page at time: `on-demand paging`)
```c 
paddr_t getppage_user(vaddr_t vadd)
void freeppage_user(paddr_t paddr)
```

#### Types of coremap entry
There are 4 possibile types of memory frames:
```c 
/* kern/include/coremap.h */
#define UNTRACKED_ENTRY 0
#define FREED_ENTRY 1
#define KERNEL_ENTRY 2                  
#define USER_ENTRY 3
```
The first 2 indicates that the current frame is free, but the difference is that `FREED_ENTRY` marks a frame that was allocated and subsequently freed, so they are the ones we search in `getfreeppages` (if no enough contiguos freed entries are found we call `ram_stealmem`).<br>
The other 2 indicates an allocated frame and what kind of process occupies it; note that a PT entry is included in `KERNEL_ENTRY` and like everyone else of this kind it cannot be swapped-out from memory.

#### Actual page replacement
As we seen before only frames marked as `USER_ENTRY` can be the victim, so they are the only can be swapped-out from memory when it is full. Page replacement is managed inside `paddr_t getppage_user` function.<br>
In this case we've chosen to implement a global replacement policy (so it doesn't matter what process is in execution) based on a First-In-First-Out algorithm. That is made using a linked list of frames;  every user frame has inside it the references to previuos and next allocated frame (fields `paddr_t prev_allocated, next_allocated` in `coremap_entry`) and a global variable traces the current victim of replacement policy.<br>
Every time a user process calls `getppage_user` to request on-demand page allocation the memory management tries to do it using usual functions (`getfreeppages` and `ram_stealmem` with 1 page); if allocation fails, so there is no space, replacement procedure is started as following: 
- current victim (which physical addres is saved in a global variable `victim`) is swapped out from memory, from current process Page Table and from TLB;
- this address is saved as the one we want to allocate for requesting process;
- using the victim `next_allocated` field we select new victim for next allocations.
```c 
paddr_t victim = 0;
[...]
paddr_t getppage_user(vaddr_t vadd){
    [...]
    //physical address of victim  (it will be our freed page to be returned)
    padd = (paddr_t)victim_tmp * PAGE_SIZE;

    //saves in offset the position in swapfile of victim
    res = swap_out(padd, &offset);
    if (res)
        panic("swap out failed\n");

    //removes from memory
    lock_acquire(as->pt_lock);
    pt_swap_out(as->pt, coremap[victim_tmp].vaddr, offset);
    lock_release(as->pt_lock);

    tlb_invalidate_entry(coremap[victim_tmp].vaddr);
    [...]
    return padd;
}
```

# Statistics
In order to track several statistics related to the performance of our virtual memory sub-system we made use of a simple vector `stats`. <br>
Each element of the structure contains the count of a different statistic and the vector is secured by the lock `stats_lock`.<br>
We enumerated the different statistics, to easily increment a specific element of the vector when the `vmstats_increment` is called.

When our virtual memory is shut down we call `vmstats_print`: this function prints to the kernel the obtained statistics and then make some consistency checks.<br>
In case these checks are not respected, the function prints a warning for each equality that doesn't hold. In this case there should be no need to acquire the lock `stats_lock`, but we still do it to add a layer of security.<br>
Lastly, `vmstats_destroy` is called and the lock is destroyed.

# Debug
To debug this project we mostly made use of gdb from command line and sometimes we used the classic debugger through VisualStudio Code.
Moreover, we used another option (`OPT_DEBUG_PAGING`) to enclose some parts of the code which are used only to check the proper behaviour of some of our functions.
