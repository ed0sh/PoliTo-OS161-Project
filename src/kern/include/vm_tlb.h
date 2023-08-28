#ifndef _VM_TLB_H_
#define _VM_TLB_H_

int tlb_get_rr_victim(void);
void tlb_load(uint32_t entryhi, uint32_t entrylo, uint32_t perm);
void tlb_invalidate(void);
void tlb_invalidate_entry(vaddr_t vaddr);

#endif 