# Project c1: Virtual Memory with Demand Paging (PAGING)
Programmazione di Sistema A.Y. 2022/2023

- Edoardo Franco (<a href="https://github.com/ed0sh" target="_blank">@ed0sh</a>)
- Filippo Gorlero (<a href="https://github.com/gorle19" target="_blank">@gorle19</a>)
- Giulia Golzio (<a href="https://github.com/jelly1600" target="_blank">@jelly1600</a>)


## Introduction and Theorical Info
- This is our implementation for **Project C1 (Paging)** of *pds_progetti_2023*. The provided set of files replaces the memory management defined in *dumbvm.c*, creating a new method based on on-demand pages requests, page tables and TLB usage, both with correct replacement and page swap-out/in algorithms
- The chosen variant of the project is **C1.1**, so we used per-process Page Tables; for the *empty regions* problem we used a linked list of segments
- **TLB** is unique inside our system and it is reserved for the current execuiting process, that is means the TLB must be invalidated at every context-swtiching

### Group management
We divided our work in 3 main areas: **TLB management**, **On-demand page loading** and **Page replacement**; each one of these was assigned to one of us who worked on his own. Every one or two days we did a online meeting where we updated the other members on our progresses and we worked together in parts of the code that had a conncection between two or more different work areas. When the code parts was about to finish we started to create stats and test our code, with provided programs (*and new ones created on purpose?*)

- For code sharing we used a GitHub repository, connected to the provided os161 Docker container 
- <a href="https://github.com/ed0sh/PoliTo-OS161-Project" target="_blank">Link to repository</a> 

## Pages and Virtual Memory
*Introduzione teorica al paging e a come funziona il tutto*

## Files and Implementation
`Note: all of new files are defined in kern/vm, while the headers file in kern/include. The changes to already existing are inclosed in #if OPT_PAGING`

### List of new files
- coremap.c
- my_vm.c
- pt.c
- segments.c
- swapfile.c
- vm_tlb.c
- vmstats.c

### List of modified files
- addrspace.c
- runprogram.c
- loadelf.c

### da qua si parte a spiegare cosa fanno i file uno per uno


# TLB Management
Concerning the management of the TLB, we made use some of the functions declared inside *tlb.h* (*tlb_probe*, *tlb_read*, *tlb_write*) and of some defined constants (*NUM_TLB*, *TLBLO_VALID*, *TLBLO_DIRTY*).
We wrote in file *vm_tlb.c* the following functions:
- *int tlb_get_rr_victim(void)*
- *void tlb_load(uint32_t entryhi, uint32_t entrylo, uint32_t perm)*
- *void tlb_invalidate(void)*
- *void tlb_invalidate_entry(vaddr_t vaddr)*

## TLB Load New Entry
The *tlb_get_rr_victim* implements a round-robin replacement policy and it returns an index indicating the next chosen victim:
```c
int tlb_get_rr_victim(void) {
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}
```

In *tlb_load* we managed the loading of a new entry inside the TLB:
- if there is unused space, highlighted thanks to a validity bit *TLBLO_VALID*, we place the new entry there;
- otherwise we choose a new victim with the round-robin algorithm, depicted above.

We gave particular attention to the case when the virtul address passed to the *vm_fault* and then to the *tlb_load* is already present in an entry of the table: in this case we take as victim such entry, to surely avoid repeated virtual addresses.
Moreover, through the parameter "perm" we discover if the new entry is writable and we take care of it setting the dirty bit *TLBLO_DIRTY*.
Here is the code:
```c
void tlb_load(uint32_t entryhi, uint32_t entrylo, uint32_t perm) {
    [...]

    index = tlb_probe(entryhi, 0);
    if (index < 0) {
        for (i = 0; i < NUM_TLB; i++) {
            tlb_read(&v_hi, &p_lo, i);
            if (!(p_lo & TLBLO_VALID)) {
                victim=i;   
                vmstats_increment(VMSTATS_TLB_FAULTS_WITH_FREE);            
                break;
            }
        }
        if(victim == -1) {  // no free entry, use round robin
            victim = tlb_get_rr_victim();
            vmstats_increment(VMSTATS_TLB_FAULTS_WITH_REPLACE);
        } 
    } else {    // vaddr already present in TLB
        victim = index;
        vmstats_increment(VMSTATS_TLB_FAULTS_WITH_REPLACE);
    }

    entrylo = entrylo | TLBLO_VALID;
    if (perm & PF_W) {  // if it is writable, set the dirty bit
        entrylo = entrylo | TLBLO_DIRTY;
    }
    tlb_write(entryhi, entrylo, victim);

    [...]

    return;
}
```

## TLB Invalidation
The function *tlb_invalidate* is called by *as_activate* to invalidate all the TLB entries. In this way we ensure that after a context-switching all TLB entries are "out-of-order": the new currently running process will be forced to load its own entries.
```c
void tlb_invalidate(void) {
    [...]

    // clear all the valid bits
    for(i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    [...]

    return;
}
```

When a page is swapped out from the page table we invalidate the corresponding TLB entry calling *tlb_invalidate_entry*. This function scrolls in the TLB looking by virtual address and then it invalidates the identified entry:
```c
void tlb_invalidate_entry(vaddr_t vaddr) {
	[...]

    // clear the valid bits
    if((i = tlb_probe(vaddr, 0)) >= 0)
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    
    [...]

    return;
}
```

## TLB Fault
We handled TLB faults inside *my_vm.c*, with *vm_fault*. This function is called by the OS when a TLB miss occurs:
- it panics in case of VM_FAULT_READONLY;
- it returns EINVAL in case the parameter faulttype is incorrect; 
- it returns EFAULT in case something is wrong with the current process, the address space or the segment;
- on success, it loads a new entry inside the TLB calling *tlb_load* and then it returns 0.

The *vm_fault* is also partial responsible for the management of the page table. The page that made the *vm_fault* be called is examined:
- if it is a stack-page we set to zero the corresponding memory area (correct?);
- otherwise we analyse it through its *page_status*:
    - if the page isn't inizialized yet (*page_status* equal to *PT_ENTRY_EMPTY*) we allocate some memory for it, we load it from the ELF program file and we add the new entry in page table;
    - if the page was swapped-out (*page_status* equal to *PT_ENTRY_SWAPPED_OUT*) we retrive it from the swapfile, performing a swap-in;
    - if the page is valid (*page_status* equal to *PT_ENTRY_VALID*) we just reload it inside the TLB.

```c
int vm_fault(int faulttype, vaddr_t faultaddress) {
    [...]

    switch(faulttype) {
        case VM_FAULT_READONLY:
		    panic("Attempt by an application to modify its text section : got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		    return EINVAL; 
    }

    if (curproc == NULL) { return EFAULT; }

    as = proc_getas();
    if (as == NULL) { return EFAULT; }

    sg = as_find_segment(as, faultaddress);
    if (sg == NULL) { return EFAULT; }


    if (sg->base_vaddr == USERSTACK - sg->mem_size) {   // it's a stack page       
        paddr = getppage_user(aligned_faultaddress);            // allocate memory 
        bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
        vmstats_increment(VMSTATS_PAGE_FAULTS_ZEROED);    
    } else {
        // Management of the page inside the PT 
        [...]
        if (page_status == PT_ENTRY_EMPTY) {                // not-initialized (0)
            paddr = getppage_user(aligned_faultaddress);            // allocate memory 
            load_page_from_elf(sg, faultaddress, paddr);
            lock_acquire(as->pt_lock);  
            pt_add_entry(pt, faultaddress, paddr, perm);    // add entry in pt  
            lock_release(as->pt_lock);           
            [...]
        } else if (page_status == PT_ENTRY_SWAPPED_OUT) {   // swapped-out (1): retrive it from swapfile 
            paddr = getppage_user(aligned_faultaddress);            // allocate memory 
            lock_acquire(as->pt_lock);  
            swap_offset = pt_get_page_swapfile_offset(pt, faultaddress);
            swap_in(paddr, swap_offset);
            pt_swap_in(pt, faultaddress, paddr, perm);
            [...]
        } else if (page_status == PT_ENTRY_VALID) {         // valid (2)
            // nothing to do
            vmstats_increment(VMSTATS_TLB_RELOADS);
        }
    }

    /* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);
    [...]

    // Management of the entry inside TLB 
    tlb_load((uint32_t)aligned_faultaddress, (uint32_t)paddr, perm);
    vmstats_increment(VMSTATS_TLB_FAULTS);

    return 0;
}
```

# On-demand page loading
On-demand page loading refers to the feature of modern kernels where process' data and code is loaded into physical as needed instead of pre-loading everything at start.<br>
To support this feature, we introduced/redefined in OS 161 structures like: **page table**, **segments** (code, data and stack) and **address space**.<br>
We also had to redefine how functions like `load_elf(...)` and `runprogram(...)` behaves.

## Page Table
### Data structure
We assumed a single-level per-process page table composed by a vector of `pt_entry` struct, each representing a page.
```c
/* kern/include/pt.h */

typedef struct _pagetable {
    uint32_t num_pages;
    vaddr_t start_vaddr;
    pt_entry* pages;
} pagetable;
```
The total number of page is defined as the sum of the pages required by the program segments and saved as `uint32_t num_pages`, while the virtual address used for the translation is the `vaddr_t start_vaddr`.
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
The page table is initialized in the `as_prepare_load(...)`, once the address space and most importantly their segments has been initialized too.<br>
The space is freed only once the process is no more running, indeed the page table deallocation function `pt_destroy(...)` is called inside `as_destroy(...)`;

#### Adress transaltion
The main point of our page table implementation is the address translation algorithm: 
```c
/* kern/include/pt.h - pt_init(...) */

pt->start_vaddr = pt_start_vaddr & PAGE_FRAME;


/* kern/include/pt.h */

vaddr_t aligned_vaddr = vaddr & PAGE_FRAME;
uint32_t pt_index = (aligned_vaddr - pt->start_vaddr) / PAGE_SIZE;
```
Each virtual address is first page-aligned, then compared with the page table `start_vaddr` and divided by `PAGE_SIZE` to get the index in the page vector, once there the path is straightforward.

## Program segments
The address space of a program contains a collection of segments that represents those read in the ELF file.<br>
In OS 161, the most important ones are: *data*, *code* and *stack*.
### Data structure

```c
/* kern/include/segments.h */

```


### Core concepts
