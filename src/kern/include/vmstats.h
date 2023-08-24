#ifndef _VMSTATS_H_
#define _VMSTATS_H_

#include <types.h>

enum {
    VMSTATS_TLB_FAULTS,
    VMSTATS_TLB_FAULTS_WITH_FREE,
    VMSTATS_TLB_FAULTS_WITH_REPLACE,
    VMSTATS_TLB_INVALIDATIONS,
    VMSTATS_TLB_RELOADS,
    VMSTATS_PAGE_FAULTS_ZEROED,
    VMSTATS_PAGE_FAULTS_DISK,
    VMSTATS_PAGE_FAULTS_ELF,
    VMSTATS_PAGE_FAULTS_SWAPFILE,
    VMSTATS_SWAPFILE_WRITES
};

#define VMSTATS_NUM 10

void vmstats_init(void);
void vmstats_increment(uint8_t stats_type);
void vmstats_print(void);
void vmstats_destroy(void);


#endif /* _VMSTATS_H_ */