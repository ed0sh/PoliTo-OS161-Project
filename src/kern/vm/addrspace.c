/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

#if OPT_PAGING

#include <synch.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <elf.h>

#endif

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(char *progname)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

#if OPT_PAGING
	as->progname = kstrdup(progname);
	if (as->progname == NULL)
		return NULL;

	if (vfs_open(progname, O_RDONLY, 0, &(as->v))) {
		kprintf("Unable to open the file %s", progname);
		kfree(as->progname);
		return NULL;
	}
	
	as->pt = NULL;
	as->pt_num_pages = 0;
	as->segments = NULL;

	as->pt_lock = lock_create("pt_lock");
	if (as->pt_lock == NULL) {
		kprintf("Unable to create page table lock");
		kfree(as);
		kfree(as->progname);
		vfs_close(as->v);
		return NULL;
	}
	
#endif
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

#if OPT_PAGING
	// TODO: dumbvm_can_sleep(); ?

	KASSERT(old != NULL);
	KASSERT(old->progname != NULL);
#endif

	newas = as_create(old->progname);
	if (newas==NULL) {
		return ENOMEM;
	}

#if OPT_PAGING
	segment *newas_curseg = NULL;
	for (segment *curseg = old->segments; curseg != NULL; curseg = curseg->next_segment) {
		segment *new_seg = init_segment(
			curseg->perm,
			curseg->base_vaddr,
			curseg->base_vaddr_offset,
			curseg->file_offset,
			curseg->file_size,
			curseg->mem_size,
			curseg->num_pages,
			NULL
		);
		if (new_seg == NULL)
			return ENOMEM;

		if (newas_curseg != NULL)
			newas_curseg->next_segment = new_seg;

		newas_curseg = new_seg;
	}

	lock_acquire(old->pt_lock);

	pagetable *new_as_pt = NULL;
	
	int pt_copy_ret_val = pt_copy(old, &new_as_pt);
	lock_release(old->pt_lock);
	
	if (pt_copy_ret_val != 0)
		return pt_copy_ret_val;

	newas->pt_num_pages = old->pt_num_pages;
#else
	(void)old;
#endif

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
#if OPT_PAGING
	// TODO: dumbvm_can_sleep(); ?

	KASSERT(as != NULL);
	KASSERT(as->v != NULL);

	if (as->v != NULL)
		vfs_close(as->v);
	
	if (as->pt != NULL && as->pt_lock != NULL) {
		lock_acquire(as->pt_lock);
		pt_destroy(as->pt);
		lock_release(as->pt_lock);
	}

	if (as->pt_lock != NULL)
		lock_destroy(as->pt_lock);
	
	if (as->segments != NULL) {
		segments_destroy_linked_list(as->segments);
	}

	if (as->progname != NULL)
		kfree(as->progname);
#endif

	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		uint32_t permissions, size_t file_size, off_t file_offset)
{
#if OPT_PAGING
	// TODO: dumbvm_can_sleep(); ?


	KASSERT(as != NULL);
	KASSERT(vaddr != 0);
	KASSERT(permissions & PF_R || permissions & PF_W || permissions & PF_X);

	size_t npages;

	/* Align the region. First, the base... */
	off_t vaddr_offset = vaddr & ~(vaddr_t)PAGE_FRAME;
	memsize += vaddr_offset;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;
	npages = memsize / PAGE_SIZE;

	segment *new_seg = segment_init(permissions, vaddr, vaddr_offset, file_offset, file_size, memsize, npages, NULL);
	if (new_seg == NULL)
		return ENOMEM;

	if (as->segments == NULL)
		as->segments = new_seg;
	else {
		segment *cur_seg;
		for (cur_seg = as->segments; cur_seg->next_segment != NULL; cur_seg = cur_seg->next_segment);
		cur_seg->next_segment = new_seg;
	}

	return 0;
#else
	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;
	return ENOSYS;
#endif
}

int
as_prepare_load(struct addrspace *as)
{
#if OPT_PAGING
	// TODO: dumbvm_can_sleep(); ?

	for (segment *curseg = as->segments; curseg != NULL; curseg = curseg->next_segment){
		as->pt_num_pages += curseg->num_pages;
	}

	// Define page table
	lock_acquire(as->pt_lock);

	segment *curseg = as->segments;
	vaddr_t base_vaddr = curseg->base_vaddr;

	while ((curseg = curseg->next_segment) != NULL) {
		if (curseg->base_vaddr < base_vaddr)
			base_vaddr = curseg->base_vaddr;
	}

	as->pt = pt_init(base_vaddr, as->pt_num_pages);

	lock_release(as->pt_lock);

	if (as->pt == NULL)
		return ENOMEM;

#else
	(void)as;
#endif
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
#if OPT_PAGING
	// TODO: dumbvm_can_sleep(); ?
#else
	(void)as;
#endif
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
#if OPT_PAGING
	// TODO: dumbvm_can_sleep(); ?

	size_t stack_size = STACK_PAGES * PAGE_SIZE;
	if (as_define_region(as, USERSTACK - stack_size, stack_size, (PF_W | PF_R), 0, 0) != 0)
		return ENOMEM;

#else
	(void)as;
#endif
	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

segment *as_find_segment(struct addrspace *as, vaddr_t vaddr) {
	KASSERT(as != NULL);
	KASSERT(as->segments != NULL);

	segment *curseg;
	for (curseg = as->segments; curseg != NULL; curseg = curseg->next_segment) {
		if (vaddr >= curseg->base_vaddr && vaddr < curseg->base_vaddr + curseg->mem_size)
			break;
	}

	return curseg;
}