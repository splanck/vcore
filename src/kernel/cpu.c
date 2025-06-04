#include "cpu.h"
#include "lib.h"

struct CPU cpus[MAX_CPU];
int cpu_count = 1;

static int current_cpu_id = 0;

struct CPU* cpu_current(void)
{
    return &cpus[current_cpu_id];
}

void cpu_init(void)
{
    memset(cpus, 0, sizeof(cpus));
    cpus[0].id = 0;
}
