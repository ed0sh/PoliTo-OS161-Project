/*  TLB MANAGEMENT  */


#include <types.h>
#include <elf.h>
#include <tlb.h>
#include <vm_tlb.h>
#include <spl.h>


// Round Robin TLB replacement algorithm
int tlb_get_rr_victim(void) {
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}


// Load a new entry in tlb (if there is space use it, otherwise free it with RR algorithm)
void tlb_load(uint32_t entryhi, uint32_t entrylo, int perm) {
    uint32_t v_hi, p_lo;
    int i, victim=-1, spl;

    // Disable interrupts on this CPU while frobbing the TLB
	spl = splhigh();

    for (i = 0; i < NUM_TLB, victim = -1; i++) {
        tlb_read(&v_hi, &p_lo, i);
        if (!(p_lo & TLBLO_VALID)) {
            victim=i;
        }
    }
    if(victim == -1) {  // no free entry, use round robin
        victim = tlb_get_rr_victim();
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
    uint32_t v_hi, p_lo;

    // Disable interrupts on this CPU while frobbing the TLB
	spl = splhigh();

    // clear all the valid bits
    for(i=0; i<NUM_TLB; i++) {
        tlb_read(&v_hi, &p_lo, i);
        if (p_lo & TLBLO_VALID) {
            p_lo = p_lo & ~(1<<TLBLO_VALID);
            tlb_write(v_hi, p_lo, i);
        }
    }

    splx(spl);
    
    #if OPT_DEBUG_PAGING
        // check if invalidation is correctly done
        for(i=0; i<NUM_TLB; i++) {
            tlb_read(&v_hi, &p_lo, i);
            if (p_lo & TLBLO_VALID) {
                panic("TLB not invalidated correctly.\n");
            }
        }
    #endif

    return;
}

