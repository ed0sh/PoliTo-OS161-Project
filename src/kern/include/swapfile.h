#ifndef SWAPFILE_H
#define SWAPFILE_H

//define current swapfile size
#define SWAP_SIZE 9*1024*1024

#define SWAP_PATH "emu0:/SWAPFILE"

//functions
int swapfile_init();
int swapfile_close();
int swap_out(paddr_t paddr, off_t *swap_offset);
int swap_in(paddr_t paddr, off_t swap_offset);          
int process_swap_free(off_t swap_offset);             

#endif