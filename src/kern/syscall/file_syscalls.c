/*
 * AUthor: G.Cabodi
 * Very simple implementation of sys_read and sys_write.
 * just works (partially) on stdin/stdout
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <current.h>
#include <lib.h>

#if OPT_FILE

#include <vnode.h>
#include <proc.h>
#include <limits.h>
#include <vfs.h>
#include <uio.h>

#define SYSTEM_OPEN_MAX (10 * OPEN_MAX)

/* system open file table */
struct openfile {
	struct vnode *vnode;
	off_t offset;
	unsigned int refcount;
};

struct openfile systemFileTable[SYSTEM_OPEN_MAX];

void openfileIncrRefCount(struct openfile *of) {
  if (of != NULL) {
    of->refcount++;
  }
}

int sys_open (userptr_t path, int openflags, mode_t mode, int* errp) {
  struct vnode *v;
  struct openfile *of = NULL;
  int result;
  int i, fd;

  result = vfs_open((char *) path, openflags, mode, &v);
  if (result != 0) {
    *errp = ENOENT;
    return -1;
  }

  for (i = 0; i < SYSTEM_OPEN_MAX; i++) {
    if (systemFileTable[i].vnode == NULL) {
      of = &systemFileTable[i];
      of->vnode = v;
      of->offset = 0;
      of->refcount = 1;
      break;
    }
  }

  if (of == NULL) {
    *errp = EMFILE;
  } else {
    for (fd = STDERR_FILENO + 1; fd < OPEN_MAX; fd++) {
      if (curproc->fileTable[fd] == NULL) {
        curproc->fileTable[fd] = of;
        return fd;
      }
    }

    // no free slot in process open file table
    *errp = EMFILE;
  }

  vfs_close(v);
  return -1;
}

int sys_close(int fd) {
  struct openfile *of;
  struct vnode *v;

  if (fd < 0 || fd > OPEN_MAX)
    return -1;
  
  of = curproc->fileTable[fd];
  if (of == NULL)
    return -1;

  curproc->fileTable[fd] = NULL;

  if (--of->refcount > 0)
    return 0;

  v = of->vnode;
  of->vnode = NULL;
  if (v == NULL)
    return -1;

  vfs_close(v);

  return 0;
}

static int file_read(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio u;
  struct openfile *of;
  struct vnode *v;
  int result;

  if (fd < 0 || fd > OPEN_MAX)
    return -1;
  
  of = curproc->fileTable[fd];
  if (of == NULL)
    return -1;
  v = of->vnode;

	iov.iov_ubase = buf_ptr;
	iov.iov_len = size;		 // length of the memory space

	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = size;          // amount to read from the file
	u.uio_offset = of->offset;
	u.uio_segflg = UIO_USERISPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = curproc->p_addrspace;

	result = VOP_READ(v, &u);
	if (result) {
		return result;
	}

  of->offset = u.uio_offset;
  return (size - u.uio_resid);
}

static int file_write(int fd, userptr_t buf_ptr, size_t size) {
  struct iovec iov;
  struct uio u;
  struct openfile *of;
  struct vnode *v;
  int result;

  if (fd < 0 || fd > OPEN_MAX)
    return -1;
  
  of = curproc->fileTable[fd];
  if (of == NULL)
    return -1;
  v = of->vnode;

	iov.iov_ubase = buf_ptr;
	iov.iov_len = size;		 // length of the memory space

	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_resid = size;          // amount to read from the file
	u.uio_offset = of->offset;
	u.uio_segflg = UIO_USERISPACE;
	u.uio_rw = UIO_WRITE;
	u.uio_space = curproc->p_addrspace;

	result = VOP_WRITE(v, &u);
	if (result) {
		return result;
	}

  of->offset = u.uio_offset;
  return (size - u.uio_resid);
}

#endif

/*
 * simple file system calls for write/read
 */
int
sys_write(int fd, userptr_t buf_ptr, size_t size)
{
  int i;
  char *p = (char *)buf_ptr;

  if (fd!=STDOUT_FILENO && fd!=STDERR_FILENO) {
#if OPT_FILE
    file_write(fd, buf_ptr, size);
#else
    kprintf("sys_write supported only to stdout\n");
    return -1;
#endif
  } else {
    for (i=0; i<(int)size; i++) {
      putch(p[i]);
    }
  }

  return (int)size;
}

int  
sys_read(int fd, userptr_t buf_ptr, size_t size)
{
  int i;
  char *p = (char *)buf_ptr;

  if (fd!=STDIN_FILENO) {
#if OPT_FILE
    file_read(fd, buf_ptr, size);
#else
    kprintf("sys_read supported only to stdin\n");
    return -1;
#endif
  } else {
    for (i=0; i<(int)size; i++) {
        p[i] = getch();
        if (p[i] < 0) 
          return i;
    }
  }

  return (int)size;
}
