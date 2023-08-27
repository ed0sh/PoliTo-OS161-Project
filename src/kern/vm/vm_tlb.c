/*  TLB MANAGEMENT  */


#include <types.h>
#include <elf.h>
#include <machine/tlb.h>
#include <vm_tlb.h>
#include <spl.h>
#include <vmstats.h>


// Round Robin TLB replacement algorithm
int tlb_get_rr_victim(void) {
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}


// Load a new entry in tlb (if there is space use it, otherwise free it with RR algorithm)
void tlb_load(uint32_t entryhi, uint32_t entrylo, uint32_t perm, int index) {
    uint32_t v_hi, p_lo;
    int i, victim=-1, spl;

    // Disable interrupts on this CPU while frobbing the TLB
	spl = splhigh();

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
    } else {
        victim = index;
        vmstats_increment(VMSTATS_TLB_FAULTS_WITH_REPLACE);
    }

    entrylo = entrylo | TLBLO_VALID;
    if (perm & PF_W) {  // if it is writable, set the dirty bit
        entrylo = entrylo | TLBLO_DIRTY;
    }

    tlb_write(entryhi, entrylo, victim);

    splx(spl);

    return;
}


void tlb_invalidate(void) {
    int i, spl;


    // Disable interrupts on this CPU while frobbing the TLB
	spl = splhigh();

    // clear all the valid bits
    for(i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
    
#if OPT_DEBUG_PAGING
    uint32_t v_hi, p_lo;
    // check if invalidation is correctly done
    for(i=0; i<NUM_TLB; i++) {
        tlb_read(&v_hi, &p_lo, i);
        if (p_lo & TLBLO_VALID) {
            panic("TLB not invalidated correctly.\n");
        }
    }
#endif

    vmstats_increment(VMSTATS_TLB_INVALIDATIONS);

    return;
}

