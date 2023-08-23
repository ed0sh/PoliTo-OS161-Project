#include <segments.h>
#include <elf.h>

segment *segment_init(uint32_t perm, vaddr_t base_vaddr, off_t base_vaddr_offset, off_t file_offset, size_t file_size, size_t mem_size, size_t num_pages, segment *next_segment) {

    KASSERT(base_vaddr != 0);
    KASSERT(num_pages > 0);
    KASSERT(perm & PF_R || perm & PF_W || perm & PF_X);

    segment *seg = kmalloc(sizeof(segment));
    seg->base_vaddr = base_vaddr;
    seg->base_vaddr_offset = base_vaddr_offset;
    seg->file_offset = file_offset;
    seg->file_size = file_size;
    seg->mem_size = mem_size;
    seg->num_pages = num_pages;
    seg->perm = perm;
    seg->next_segment = next_segment;

    return seg;
}

void segment_destroy(segment *seg) {
    while (seg->next_segment->next_segment != NULL) {
        seg = seg->next_segment;
    }

    kfree(seg->next_segment);
    seg->next_segment = NULL;
}

void segments_destroy_linked_list(segment *seg) {
    segment *int_seg;
    
    while (seg->next_segment != NULL) {
        int_seg = seg;
        segment_destroy(int_seg);
    }

    kfree(seg);
}