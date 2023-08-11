#include <pt.h>
#include <elf.h>
#include <vm.h>

pagetable *pt_init(vaddr_t pt_start_vaddr, uint32_t pt_num_pages) {
    
    uint32_t i;

    KASSERT(pt_num_pages != 0);

    pagetable *pt = kmalloc(sizeof(pagetable));

    if (pt == NULL)
        return NULL;
    
    pt->num_pages = pt_num_pages;
    pt->start_vaddr = pt_start_vaddr & PAGE_FRAME;
    pt->pages = kmalloc(pt_num_pages * sizeof(pt_entry));

    if (pt->pages == NULL) {
        pt->num_pages = 0;
        return NULL;
    }

    for (i = 0; i < pt_num_pages; i++) {
        pt->pages[i].status = PT_ENTRY_EMPTY;
    }

    return pt;
}

void pt_add_entry(pagetable *pt, vaddr_t vaddr, paddr_t paddr, uint32_t perm) {
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr - pt->start_vaddr <= PAGE_SIZE * pt->num_pages);

    vaddr_t aligned_vaddr = vaddr & PAGE_FRAME;
    uint32_t pt_index = (aligned_vaddr - pt->start_vaddr) / PAGE_SIZE;
    
    KASSERT(pt_index < pt->num_pages);    
    KASSERT(pt->pages[pt_index].status == PT_ENTRY_EMPTY || pt->pages[pt_index].status == PT_ENTRY_SWAPPED_OUT);

    pt->pages[pt_index].paddr = aligned_vaddr;
    pt->pages[pt_index].perm = perm;
    pt->pages[pt_index].status = PT_ENTRY_VALID;
}

uint8_t pt_get_page(pagetable *pt, vaddr_t vaddr, paddr_t *paddr, uint32_t *perm) {
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr - pt->start_vaddr <= PAGE_SIZE * pt->num_pages);

    vaddr_t aligned_vaddr = vaddr & PAGE_FRAME;
    uint32_t pt_index = (aligned_vaddr - pt->start_vaddr) / PAGE_SIZE;

    if (pt->pages[pt_index].status == PT_ENTRY_VALID) {
        paddr = pt->pages[pt_index].paddr;
        perm = pt->pages[pt_index].perm;
    } else {
        paddr = NULL;
        perm = NULL;
    }
    
    return pt->pages[pt_index].status;
}

void pt_swap_in(pagetable *pt, vaddr_t vaddr, paddr_t paddr, uint32_t perm) {
    pt_add_entry(pt, vaddr, paddr, perm);
}

void pt_swap_out(pagetable *pt, vaddr_t vaddr) {
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr - pt->start_vaddr <= PAGE_SIZE * pt->num_pages);

    vaddr_t aligned_vaddr = vaddr & PAGE_FRAME;
    uint32_t pt_index = (aligned_vaddr - pt->start_vaddr) / PAGE_SIZE;

    KASSERT(pt->pages[pt_index].status == PT_ENTRY_VALID);

    pt->pages[pt_index].status = PT_ENTRY_SWAPPED_OUT;
}

void pt_destroy(pagetable *pt) {
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    kfree(pt->pages);
    kfree(pt);
}  