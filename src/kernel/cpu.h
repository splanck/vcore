#ifndef _CPU_H_
#define _CPU_H_

#include "process.h"

#define MAX_CPU 4

struct CPU {
    int id;
    int online;
    struct ProcessControl pc;
};

extern struct CPU cpus[MAX_CPU];
extern int cpu_count;
extern int cpu_online_count;

void cpu_init(void);
void cpu_mark_online(int id);
struct CPU* cpu_current(void);
void reschedule_other_cpus(void);
void tlb_shootdown(void);

#endif
