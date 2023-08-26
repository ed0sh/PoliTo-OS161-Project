#include <vmstats.h>
#include <synch.h>
#include <lib.h>

static unsigned int stats[VMSTATS_NUM];
static unsigned int stats_initialized = 0;

static struct lock *stats_lock;

static const char *stats_names[] = {
  "TLB Faults", 
  "TLB Faults with Free",
  "TLB Faults with Replace",
  "TLB Invalidations",
  "TLB Reloads",
  "Page Faults (Zeroed)",
  "Page Faults (Disk)",
  "Page Faults from ELF",
  "Page Faults from Swapfile",
  "Swapfile Writes"
};

void vmstats_init(void) {
    stats_lock = lock_create("stats_lock");
    if (stats_lock == NULL)
        panic("Error while creating stats lock.\n");

    for (int i = 0; i < VMSTATS_NUM; i++) {
        stats[i] = 0;
    }

    stats_initialized = 1;
}

void vmstats_increment(uint8_t stats_type) {
    KASSERT(stats_type < VMSTATS_NUM);
    KASSERT(stats_initialized);

    lock_acquire(stats_lock);

    stats[stats_type]++;

    lock_release(stats_lock);
}

void vmstats_print(void) {
    KASSERT(stats_initialized);
    
    // There should be no need to acquire the lock given that this function should only be called while shutting down the vm
    // but we still add a layer of security.
    lock_acquire(stats_lock);

    kprintf("Virtual Memory with Demand Paging statistics\n\n");
    kprintf("\tStats type\t|\tValue\n");

    for (int i = 0; i < VMSTATS_NUM; i++) {
        kprintf("\t%s\t|\t%d\n", stats_names[i], stats[i]);
    }

    // Statistics consistency checks
    kprintf("\n--- CONSISTENCY CHECKS ---\n\n");

    // “TLB Faults” = “TLB faults with free” + “TLB faults with replacement”
    unsigned int tlb_faults_free_replace = stats[VMSTATS_TLB_FAULTS_WITH_FREE] + stats[VMSTATS_TLB_FAULTS_WITH_REPLACE];

    if (stats[VMSTATS_TLB_FAULTS] != tlb_faults_free_replace)
        kprintf("WARNING: TLB Faults != TLB Faults with Free + TLB Faults with Replace\n\t--> %d != %d\n", stats[VMSTATS_TLB_FAULTS], tlb_faults_free_replace);
    else
        kprintf("INFO: TLB Faults with Free + TLB Faults with Replace = %d\n\t--> Correct!\n", tlb_faults_free_replace);

    // “TLB Faults” = “TLB Reloads” + “Page Faults (Disk)” + “Page Faults (Zeroed)”
    unsigned int tlb_faults_disk_zeroed_reaload =  stats[VMSTATS_TLB_RELOADS] + stats[VMSTATS_PAGE_FAULTS_DISK] + stats[VMSTATS_PAGE_FAULTS_ZEROED];
    
    if (stats[VMSTATS_TLB_FAULTS] != tlb_faults_disk_zeroed_reaload)
        kprintf("WARNING: TLB Faults != TLB Reloads + Page Faults (Zeroed) + Page Faults (Disk)\n\t--> %d != %d\n", stats[VMSTATS_TLB_FAULTS], tlb_faults_disk_zeroed_reaload);
    else
        kprintf("INFO: TLB Reloads + Page Faults (Zeroed) + Page Faults (Disk) = %d\n\t--> Correct!\n", tlb_faults_disk_zeroed_reaload);

    // “Page Faults (Disk)” = “Page Faults from ELF” + “Page Faults from Swapfile”
    unsigned int page_fault_disk_elf_swapfile = stats[VMSTATS_PAGE_FAULTS_ELF] + stats[VMSTATS_PAGE_FAULTS_SWAPFILE];

    if (stats[VMSTATS_PAGE_FAULTS_DISK] != page_fault_disk_elf_swapfile)
        kprintf("WARNING: Page Faults (Disk) != ELF File reads + Swapfile reads\n\t--> %d != %d\n", stats[VMSTATS_PAGE_FAULTS_DISK], page_fault_disk_elf_swapfile);
    else
        kprintf("INFO: ELF File reads + Swapfile reads = %d\n\t--> Correct!\n", page_fault_disk_elf_swapfile);

    lock_release(stats_lock);
}

void vmstats_destroy(void) {
    KASSERT(stats_initialized);

    lock_destroy(stats_lock);
    stats_initialized = 0;
}