#ifndef _PT_H_
#define _PT_H_

#include <types.h>

#define PT_ENTRY_EMPTY 0
#define PT_ENTRY_SWAPPED_OUT 1
#define PT_ENTRY_VALID 2

typedef struct _pt_entry {
    uint8_t status;         // Page table entry status: not-initialized (0), swapped-out (1), valid (2)
    paddr_t paddr;          // Memory physical address
    uint32_t perm;          // Page permissions: RWX
    off_t swapfile_offset;  // Page offset in the SWAPFILE
} pt_entry;

typedef struct _pagetable {
    uint32_t num_pages1;     // Number of pages in the page table 1
    uint32_t num_pages2;     // Number of pages in the page table 2
    vaddr_t start_vaddr1;    // Page table start address 1
    vaddr_t start_vaddr2;    // Page table start address 2
    pt_entry* pages;        // Page table entries
} pagetable;


pagetable *pt_init(vaddr_t pt_start_vaddr1, uint32_t pt_num_pages1, vaddr_t pt_start_vaddr2, uint32_t pt_num_pages2);
int pt_copy(pagetable *old, pagetable **ret);
void pt_add_entry(pagetable *pt, vaddr_t vaddr, paddr_t paddr, uint32_t perm);
uint8_t pt_get_page(pagetable *pt, vaddr_t vaddr, paddr_t *paddr, uint32_t *perm);
void pt_swap_in(pagetable *pt, vaddr_t vaddr, paddr_t paddr, uint32_t perm);
void pt_swap_out(pagetable *pt, vaddr_t vaddr, off_t swapfile_offset);
void pt_destroy(pagetable *pt);
off_t pt_get_page_swapfile_offset(pagetable *pt, vaddr_t vaddr);

#endif