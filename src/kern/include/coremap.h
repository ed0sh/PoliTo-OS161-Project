#ifndef _COREMAP_H_
#define _COREMAP_H_

#define UNTRACKED_ENTRY 0
#define FREED_ENTRY 1
#define KERNEL_ENTRY 2                  //pt entry is here, cannot be swapped-out
#define USER_ENTRY 3

#include <types.h>

//represents a single physical page in memory
struct coremap_entry {
    int type;
    int alloc_size;
    vaddr_t vaddr;
    struct addrspace *as;
    //only for user space
    paddr_t prev_allocated, next_allocated;
};

int coremap_init(void);
int coremap_close(void);

vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);
paddr_t getppage_user(vaddr_t vaddr);
void freeppage_user(paddr_t paddr);


#endif