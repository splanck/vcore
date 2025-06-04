#include "cpu.h"
#include "lib.h"
#include "arch/x86/smp.h"
#include "memory.h"

struct CPU cpus[MAX_CPU];
int cpu_count = 1;
int cpu_online_count = 0;

static int current_cpu_id = 0;

static int detect_cpus(void)
{
    unsigned int eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1), "c"(0));

    int n = (ebx >> 16) & 0xff;
    if (n < 1)
        n = 1;
    if (n > MAX_CPU)
        n = MAX_CPU;
    return n;
}

struct CPU* cpu_current(void)
{
    return &cpus[current_cpu_id];
}

void cpu_mark_online(int id)
{
    if (!cpus[id].online) {
        cpus[id].online = 1;
        cpu_online_count++;
    }
    current_cpu_id = id;
}

static void broadcast_ipi(unsigned char vec)
{
    for (int i = 0; i < cpu_count; i++) {
        if (!cpus[i].online || i == current_cpu_id)
            continue;
        send_ipi(i, vec);
    }
}

void reschedule_other_cpus(void)
{
    broadcast_ipi(40);
}

void tlb_shootdown(void)
{
    broadcast_ipi(41);
}

void cpu_init(void)
{
    memset(cpus, 0, sizeof(cpus));
    cpu_count = detect_cpus();
    for (int i = 0; i < cpu_count; i++)
        cpus[i].id = i;
    cpu_mark_online(0);
}
