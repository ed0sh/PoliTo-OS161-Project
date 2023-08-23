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

int coremap_init();
int coremap_close();
static int isCoremapActive();
static paddr_t search_free_pages(int npages);

//funzioni per la gestione dell'allocazione di memoria
// TODO: controllare quali funzioni vanno definite qua
static paddr_t getfreeppages(unsigned long npages, int entry_type, struct addrspace *as, vaddr_t vadd);
static paddr_t getppages(unsigned long npages);
static int freeppages(paddr_t addr, unsigned long npages);

vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);
paddr_t alloc_getppage_userupage(vaddr_t vaddr);
void free_freeppage_userupage(paddr_t paddr);


#endif