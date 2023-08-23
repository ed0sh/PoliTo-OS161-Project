/* TLB MANAGEMENT */

// Round Robin TLB replacement algorithm
int tlb_get_rr_victim(void);

uint32_t tlb_search(uint32_t entryhi);
void tlb_load(pt_entry *pt_entry, uint32_t entryhi);
void tlb_invalidate(void);
int vm_fault(int faulttype, vaddr_t faultaddress);
static void my_vm_can_sleep(void);
