#include <pt.h>
#include <elf.h>
#include <vm.h>
#include <kern/errno.h>
#include <lib.h>

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
        kfree(pt);
        return NULL;
    }

    for (i = 0; i < pt_num_pages; i++) {
        pt->pages[i].status = PT_ENTRY_EMPTY;
    }

    return pt;
}

int pt_copy(pagetable *old, pagetable **ret) {
    KASSERT(old != NULL);

    pagetable *new_pt = pt_init(old->start_vaddr, old->num_pages);
    if (new_pt == NULL)
        return ENOMEM;

    for (uint32_t i = 0; i < old->num_pages; i++) {
        new_pt->pages[i].paddr = old->pages[i].paddr;
        new_pt->pages[i].perm = old->pages[i].perm;
        new_pt->pages[i].status = old->pages[i].status;
    }

    // Avoid parameter set but not used
    (void)ret;

    ret = &new_pt;
    return 0;

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

    pt->pages[pt_index].paddr = paddr;
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
        *paddr = pt->pages[pt_index].paddr;
        *perm = pt->pages[pt_index].perm;
    } else {
        paddr = NULL;
        perm = NULL;
    }

    // Avoid parameter set but not used
    (void)paddr;
    (void)perm;
    
    return pt->pages[pt_index].status;
}

void pt_swap_in(pagetable *pt, vaddr_t vaddr, paddr_t paddr, uint32_t perm) {
    pt_add_entry(pt, vaddr, paddr, perm);
}

void pt_swap_out(pagetable *pt, vaddr_t vaddr, off_t swapfile_offset) {
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr - pt->start_vaddr <= PAGE_SIZE * pt->num_pages);

    vaddr_t aligned_vaddr = vaddr & PAGE_FRAME;
    uint32_t pt_index = (aligned_vaddr - pt->start_vaddr) / PAGE_SIZE;

    KASSERT(pt->pages[pt_index].status == PT_ENTRY_VALID);

    pt->pages[pt_index].status = PT_ENTRY_SWAPPED_OUT;
    pt->pages[pt_index].swapfile_offset = swapfile_offset;
}

void pt_destroy(pagetable *pt) {
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    kfree(pt->pages);
    kfree(pt);
}

off_t pt_get_page_swapfile_offset(pagetable *pt, vaddr_t vaddr) {
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr - pt->start_vaddr <= PAGE_SIZE * pt->num_pages);

    vaddr_t aligned_vaddr = vaddr & PAGE_FRAME;
    uint32_t pt_index = (aligned_vaddr - pt->start_vaddr) / PAGE_SIZE;

    KASSERT(pt->pages[pt_index].status == PT_ENTRY_SWAPPED_OUT);

    return pt->pages[pt_index].swapfile_offset;
}