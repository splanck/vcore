#ifndef _CPU_H_
#define _CPU_H_

#include "process.h"

#define MAX_CPU 4

struct CPU {
    int id;
    struct ProcessControl pc;
};

extern struct CPU cpus[MAX_CPU];
extern int cpu_count;

void cpu_init(void);
struct CPU* cpu_current(void);
void reschedule_other_cpus(void);
void tlb_shootdown(void);

#endif
