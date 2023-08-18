#ifndef _SEGMENTS_H_
#define _SEGMENTS_H_

#include <types.h>

typedef struct _segment {
    uint32_t perm;              // Segment permissions
    vaddr_t base_vaddr;         // Aligned vaddr
    off_t base_vaddr_offset;    // The offset coming from the alignment
    off_t file_offset;          // The segment offset within the file
    size_t file_size;           // The size of the data within the file
    size_t mem_size;            // Size of data to be loaded into memory
    size_t num_pages;           // Number of pages occupied by the current segment
    segment *next_segment;      // Next program segment 
} segment;

segment *segment_init(uint32_t perm, vaddr_t base_vaddr, off_t base_vaddr_offset, off_t file_offset, size_t file_size, size_t mem_size, size_t num_pages, segment *next_segment);
void segment_destroy(segment *seg);
void segments_destroy_linked_list(segment *seg);

#endif