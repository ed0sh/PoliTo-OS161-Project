/*   VM MANAGEMENT   */

#include <kern/errno.h>
#include <types.h>
#include <current.h>
#include <cpu.h>
#include <machine/tlb.h>
#include <vm.h>
#include <synch.h>
#include <proc.h>

#include <coremap.h>
#include <swapfile.h>
#include <segments.h>
#include <pt.h>
#include <my_vm.h>
#include <vm_tlb.h>
#include <addrspace.h>
#include <vmstats.h>
#include "opt-debug_paging.h"


void vm_bootstrap(void) {
    swapfile_init();
    vmstats_init();
}


void vm_shutdown(void){
    swapfile_close();
    vmstats_print();
    vmstats_destroy();
}


void vm_tlbshootdown(const struct tlbshootdown *ts) {
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}


// Check if we're in a context that can sleep
void vm_can_sleep(void) {
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
    uint32_t perm;
	paddr_t paddr;
	struct addrspace *as;
    segment *sg;
    pagetable *pt;

    vaddr_t aligned_faultaddress = faultaddress & PAGE_FRAME;

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

    sg = as_find_segment(as, faultaddress);
    if (sg == NULL) {
        return EFAULT;
    }

    if (sg->base_vaddr == USERSTACK - sg->mem_size) {       // it's a stack page
        
        paddr = getppage_user(aligned_faultaddress);
        bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);
        vmstats_increment(VMSTATS_PAGE_FAULTS_ZEROED);
    
    } else {

        // Management of the page inside the PT 
        pt = as->pt;

        lock_acquire(as->pt_lock);
        page_status = pt_get_page(pt, faultaddress, &paddr, &perm);
        lock_release(as->pt_lock);

        if (page_status == PT_ENTRY_EMPTY) {                // not-initialized (0)
            paddr = getppage_user(aligned_faultaddress); 

            load_page_from_elf(sg, faultaddress, paddr);

            lock_acquire(as->pt_lock);  
            pt_add_entry(pt, faultaddress, paddr, perm);
            lock_release(as->pt_lock);
            
            vmstats_increment(VMSTATS_PAGE_FAULTS_ELF);
            vmstats_increment(VMSTATS_PAGE_FAULTS_DISK);

        } else if (page_status == PT_ENTRY_SWAPPED_OUT) {   // swapped-out (1)
            paddr = getppage_user(aligned_faultaddress);
            lock_acquire(as->pt_lock);  

            swap_offset = pt_get_page_swapfile_offset(pt, faultaddress);
            swap_in(paddr, swap_offset);
            pt_swap_in(pt, faultaddress, paddr, perm);

            lock_release(as->pt_lock);

            vmstats_increment(VMSTATS_PAGE_FAULTS_DISK);

        } else if (page_status == PT_ENTRY_VALID) {         // valid (2)
            // nothing to do
            vmstats_increment(VMSTATS_TLB_RELOADS);
        }

    }

    /* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

#if OPT_DEBUG_PAGING
    uint32_t v_hi, p_lo;
    uint32_t count_valid = 0;
    uint32_t count_invalid = 0;
    // check number of valid and invalid TLB entries
    for(int i = 0; i < NUM_TLB; i++) {
        tlb_read(&v_hi, &p_lo, i);
        if (p_lo & TLBLO_VALID) 
            count_valid++;
        else
            count_invalid++;
    }
    
    kprintf("TLB status: %d valid - %d invalid\n", count_valid, count_invalid);
#endif

    // Management of the entry inside TLB 
    tlb_load((uint32_t)aligned_faultaddress, (uint32_t)paddr, perm);

    vmstats_increment(VMSTATS_TLB_FAULTS);

    return 0;
}
