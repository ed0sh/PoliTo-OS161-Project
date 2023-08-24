#ifndef _MY_VM_H_
#define _MY_VM_H_

void vm_bootstrap(void);
void vm_shutdown(void);
void vm_tlbshootdown(const struct tlbshootdown *ts);
static void vm_can_sleep(void);

#endif 