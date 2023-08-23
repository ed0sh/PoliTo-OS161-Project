/*  TLB MANAGEMENT

    Use of: tlb_random, tlb_write, tlb_read, tlb_probe
    
    Virtual Page in ENTRYHI, Physical Page in ENTRYLO

    "spl" stands for "set priority level"
    aggiungi da dumbvm.c  splhigh e splx rige 320-180 tipo --> servono ogni volta che fai tlb_write
*/

#include <types.h>
#include <elf.h>
#include <tlb.h>
#include <vm_tlb.h>

// round robin algorithm for TLB replacement
int tlb_get_rr_victim(void)
{
    int victim;
    static unsigned int next_victim = 0;
    victim = next_victim;
    next_victim = (next_victim + 1) % NUM_TLB;
    return victim;
}

// Search in TLB - return physical page 
uint32_t tlb_search(uint32_t entryhi)
{
    int index;
    uint32_t v_hi, p_lo;
    index = tlb_probe(entryhi, 0);
    if (index < 0)
    { // TLB MISS
        // call TLB miss exception (EX_TLBL o EX_TLBS) - (you must go in pt and then eventually in memory)
        vm_fault(EX_TLBL, v_hi); 
    }
    else
    { // TLB HIT
        tlb_read(&v_hi, &p_lo, index);
        if (!(p_lo & TLBLO_VALID))  // invalid
        { 
            // call TLB miss exception (EX_TLBL o EX_TLBS) - (you must go in pt and then eventually in memory)
            vm_fault(EX_TLBL, v_hi); 
        }

        #if OPT_DEBUG_PAGING
            if (v_hi != entryhi)
            {
                panic("TLB search not working right.\n");
            }
        #endif

        return p_lo;
    }
}

// Load a new entry in tlb (if there is space use it, otherwise free it with RR algorithm)
void tlb_load(pt_entry *pt_entry, uint32_t entryhi){ //vedi meglio per cosa passare come argomenti
    uint32_t v_hi, p_lo, perm, entrylo;
    int i, victim=-1, spl;

    // Disable interrupts on this CPU while frobbing the TLB
	spl = splhigh();

    entrylo = (uint32_t) pt_entry->paddr;
    perm = pt_entry->perm;

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

// La funzione vm_fault() era definita in dumbvm.c --> dobbiamo rifarla!! 
// La vm_fault() viene usata nel file trap.c in mips_trap().
// Gestione degli errori:
// EX_MOD    1  TLB Modify (write to read-only page) 
// EX_TLBL   2  TLB miss on load 
// EX_TLBS   3  TLB miss on store 
int vm_fault(int faulttype, vaddr_t faultaddress){

}

// Check if we're in a context that can sleep - Assert that sleeping is ok
static void my_vm_can_sleep(void) {
    if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}
