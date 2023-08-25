# Project c1: Virtual Memory with Demand Paging (PAGING)
Programmazione di Sistema A.Y. 2022/2023

- Edoardo Franco (<a href="https://github.com/ed0sh" target="_blank">@ed0sh</a>)
- Filippo Gorlero (<a href="https://github.com/gorle19" target="_blank">@gorle19</a>)
- Giulia Golzio (<a href="https://github.com/jelly1600" target="_blank">@jelly1600</a>)


## Introduction and Theorical Info
- This is our implementation for **Project C1 (Paging)** of *pds_progetti_2023*. The provided set of files replaces the memory management defined in *dumbvm.c*, creating a new method based on on-demand pages requests, page tables and TLB usage, both with correct replacement and page swap-out/in algorithms
- The chosen variant of the project is **C1.1**, so we used per-process Page Tables; for the *empty regions* problem we used a linked list of segments
- **TLB** is unique inside our system and it is reserved for the current execuiting process, that is means the TLB must be invalidated at every context-swtiching

### Group management
We divided our work in 3 main areas: **TLB management**, **On-demand page loading** and **Page replacement**; each one of these was assigned to one of us who worked on his own. Every one or two days we did a online meeting where we updated the other members on our progresses and we worked together in parts of the code that had a conncection between two or more different work areas. When the code parts was about to finish we started to create stats and test our code, with provided programs (*and new ones created on purpose?*)

- For code sharing we used a GitHub repository, connected to the provided os161 Docker container 
- <a href="https://github.com/ed0sh/PoliTo-OS161-Project" target="_blank">Link to repository</a> 

## Pages and Virtual Memory
*Introduzione teorica al paging e a come funziona il tutto*

## Files and Implementation
`Note: all of new files are defined in kern/vm, while the headers file in kern/include. The changes to already existing are inclosed in #if OPT_PAGING`

### List of new files
- coremap.c
- my_vm.c
- pt.c
- segments.c
- swapfile.c
- vm_tlb.c
- vmstats.c

### List of modified files
- addrspace.c
- runprogram.c
- loadelf.c

### da qua si parte a spiegare cosa fanno i file uno per uno