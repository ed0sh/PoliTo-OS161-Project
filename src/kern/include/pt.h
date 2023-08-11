#ifndef _PT_H_
#define _PT_H_

#include <types.h>

#define PT_ENTRY_EMPTY 0
#define PT_ENTRY_SWAPPED_OUT 1
#define PT_ENTRY_VALID 2

typedef struct _pt_entry {
    uint8_t status;
    paddr_t paddr;
    uint32_t perm;
} pt_entry;

typedef struct _pagetable {
    uint32_t num_pages;
    vaddr_t start_vaddr;
    pt_entry* pages;
} pagetable;


pagetable *pt_init(vaddr_t pt_start_vaddr, uint32_t pt_num_pages);
void pt_add_entry(pagetable *pt, vaddr_t vaddr, paddr_t paddr, uint32_t perm);
uint8_t pt_get_page(pagetable *pt, vaddr_t vaddr, paddr_t *paddr, uint32_t *perm);
void pt_swap_in(pagetable *pt, vaddr_t vaddr, paddr_t paddr, uint32_t perm);
void pt_swap_out(pagetable *pt, vaddr_t vaddr);
void pt_destroy(pagetable *pt);

#endif