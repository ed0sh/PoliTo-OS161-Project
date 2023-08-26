#include <types.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <vm.h>
#include <bitmap.h>
#include <swapfile.h>
#include <vmstats.h>


//spinlock for mutex to swapfile and bitmap
struct spinlock swapfile_lock = SPINLOCK_INITIALIZER;

//vnode for swapfile
struct vnode *swapfile;

//bitmap 
struct bitmap *swapfile_map;


/*
initializes a void swapfile, with defined dimension (default 9 MB)
creates the associate bitmap (one entry for swapfile "page")
*/
int swapfile_init(void){
    int res;

    char *path = kstrdup(SWAP_PATH);
    res = vfs_open(path, O_RDWR | O_CREAT, 0, &swapfile);
    if (res)
        panic("Failed to open swapfile\n");
    
    swapfile_map = bitmap_create(SWAP_SIZE/PAGE_SIZE);
    
    return 0;
}


/*
closes the swapfile and de-allocates the bitmap
*/
int swapfile_close(void){
    if (swapfile == NULL || swapfile_map == NULL)
        panic("Trying to close a null swapfile\n");

    vfs_close(swapfile);
    bitmap_destroy(swapfile_map);

    return 0;
}


/*
performs the swap-out operation (victim page from coremap to swapfile)
error with panic if the swapfile is full
parameters: physical address of page to swap-out, pointer to swapfile offset where the page will be saved
*/
int swap_out(paddr_t paddr, off_t *swap_offset){
    unsigned int index;
    int res;
    off_t offset;
    struct iovec iov;
    struct uio u;
    
    KASSERT(paddr != 0);

    //search for free space in swapfile, using bitmap
    spinlock_acquire(&swapfile_lock);
    res = bitmap_alloc(swapfile_map, &index);
    if (res)
        panic("Out of swap space\n");
    spinlock_release(&swapfile_lock);

    //select swapfile address
    offset = index * PAGE_SIZE;   
    if (offset > SWAP_SIZE)
        panic("Swapfile's page out of bound\n");

    //write swapped-out page in offset position of swapfile (temporary parking when memory is full)
    //PADDR_TO_KVADDR --> converts paddr to a virtual one, by adding 2GB
    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, offset, UIO_WRITE);
    VOP_WRITE(swapfile, &u);
    if (u.uio_resid != 0) {
        panic("Cannot write page to swapfile\n");
    }

    //save position in swapfile in order to get it back later directly
    *swap_offset = offset;

    vmstats_increment(VMSTATS_SWAPFILE_WRITES);

    return 0;
}


/*
swap-in operation, so remove a page from swapfile (invalid the index of bitmap)
parameters: physical address of page to swap-in, page offset in swapfile in order to select it directly 
*/
int swap_in(paddr_t paddr, off_t swap_offset){
    int index;
    struct iovec iov;
    struct uio u;

    KASSERT(swap_offset < SWAP_SIZE);

    //found bitmap index
    index = swap_offset / PAGE_SIZE;

    //check if index is valid 
    spinlock_acquire(&swapfile_lock);
    if (!bitmap_isset(swapfile_map, index)) 
        panic("No swapped pages found at this address\n");
    spinlock_release(&swapfile_lock);

    //trying to read at that address (no delete, just invalid the bitmap)
    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, swap_offset, UIO_READ);
    VOP_READ(swapfile, &u);
    if (u.uio_resid != 0) 
        panic("Failed to read the requested page from swapfile\n");

    spinlock_acquire(&swapfile_lock);
    bitmap_unmark(swapfile_map, index);
    spinlock_release(&swapfile_lock);

    vmstats_increment(VMSTATS_PAGE_FAULTS_SWAPFILE);

    return 0;
}


/*
remove a single page of a process (clear remaining process swapfile entries when it terminates)
parameters: page offset in swapfile in order to select it directly 
*/
int process_swap_free(off_t swap_offset){
    int index;

    KASSERT(swap_offset < SWAP_SIZE);    

    //found bitmap index
    index = swap_offset / PAGE_SIZE;

    //check if index is valid 
    spinlock_acquire(&swapfile_lock);
    if (!bitmap_isset(swapfile_map, index)) 
        panic("No swapped pages found at this address\n");
    spinlock_release(&swapfile_lock);

    //free entry from bitmap
    spinlock_acquire(&swapfile_lock);
    bitmap_unmark(swapfile_map, index);
    spinlock_release(&swapfile_lock);

    return 0;
}           
