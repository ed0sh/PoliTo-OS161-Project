/*   VM MANAGEMENT   */

#include <types.h>
#include <coremap.h>
#include <swapfile.h>
#include <vm.h>
#include <segments.h>
#include <pt.h>
#include <my_vm.h>
#include <addrspace.h>
#include <vmstats.h>


void vm_bootstrap(void) {
	coremap_init();
    swapfile_init();
    vmstats_init();
}


void vm_shutdown(void){
    coremap_close();
    swapfile_close();
    vmstats_print();
    vmstats_destroy();
}


void vm_tlbshootdown(const struct tlbshootdown *ts) {
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}


// Check if we're in a context that can sleep
static void vm_can_sleep(void) {
    if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}


// Function vm_fault() is called inside "mips_trap()" in file "trap.c"
int vm_fault(int faulttype, vaddr_t faultaddress) {
    uint8_t page_status;
    off_t swap_offset;
	int index; 
    uint32_t perm, v_hi, p_lo;
	paddr_t paddr;
	struct addrspace *as;
    segment *sg;
    pagetable *pt;


    switch(faulttype) {
        case VM_FAULT_READONLY:
		    panic("Attempt by an application to modify its text section : got VM_FAULT_READONLY\n");
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		    return EINVAL; 
    }

    if (curproc == NULL) {
        return EFAULT;
    }

    as = proc_getas();
    if (as == NULL) {
        return EFAULT;
    }

    lock_acquire(as->pt_lock);
    sg = as_find_segment(as, faultaddress); 
    lock_release(as->pt_lock);
    if (sg == NULL) {
        return EFAULT;
    }


    // Management of the page inside the PT 
    lock_acquire(as->pt_lock);
    pt = as->pt;
    page_status = pt_get_page(pt, faultaddress, &paddr, &perm);
    lock_release(as->pt_lock);
    if (page_status == PT_ENTRY_EMPTY) {                // not-initialized (0)
        paddr = getppage_user(faultaddress);            // allocate memory 
        lock_acquire(as->pt_lock);  
        load_page_from_elf(sg, faultaddress, paddr);    
        if (sg->base_vaddr == USERSTACK - sg->mem_size) {   // it's a stack page
            bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);

        }
        pt_add_entry(pt, faultaddress, paddr, perm);    // add entry in pt  
        lock_release(as->pt_lock);
    } else if (page_status == PT_ENTRY_SWAPPED_OUT) {   // swapped-out (1): retrive it from swapfile 
        lock_acquire(as->pt_lock);  
        swap_offset = pt_get_page_swapfile_offset(pt, faultaddress);
        swap_in(paddr, swap_offset);
        pt_swap_in(pt, faultaddress, paddr, perm);
        lock_release(as->pt_lock);
    } else if (page_status == PT_ENTRY_VALID) {         // valid (2)
        // nothing to do // controlla cosa fanno gli altri
    }

    /* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);


    // Management of the entry inside TLB 
    index = tlb_probe((uint32_t)faultaddress, 0);
    if (index < 0) {
        tlb_load(faultaddress, paddr, perm);
    } else {
        tlb_read(&v_hi, &p_lo, index);
        if (!(p_lo & TLBLO_VALID) || v_hi =! (uint32_t) faultaddress || p_lo =! (uint32_t)paddr) {
            tlb_load(faultaddress, paddr, perm);
        }
    }


    return 0;
}
