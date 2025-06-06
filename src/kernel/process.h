#ifndef _PROCESS_H_
#define _PROCESS_H_

#include "trap.h"
#include "lib.h"
#include "file.h"

struct Process {
        struct List *next;
    int pid;
        int state;
        int wait;
    int priority;
    int cpu_id;
    int time_slice;
    uint64_t runtime;
        struct FileDesc *file[100];
        uint64_t context;
        uint64_t page_map;
        uint64_t stack;
        struct TrapFrame *tf;
        uint64_t brk;
};

struct TSS {
    uint32_t res0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
	uint64_t res1;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t res2;
	uint16_t res3;
	uint16_t iopb;
} __attribute__((packed));

struct SchedInfo {
    int pid;
    int priority;
    uint64_t runtime;
    int time_slice;
};


#define MAX_PRIORITY 4

struct ProcessControl {
    struct Process *current_process;
    struct HeadList ready_list[MAX_PRIORITY];
    struct HeadList wait_list;
    struct HeadList kill_list;
    int need_resched;
};

#define STACK_SIZE (2*1024*1024)
#define NUM_PROC 10
#define PROC_UNUSED 0
#define PROC_INIT 1
#define PROC_RUNNING 2
#define PROC_READY 3
#define PROC_SLEEP 4
#define PROC_KILLED 5

void set_process_priority(struct Process *proc, int priority);

void init_process(void);
struct ProcessControl* get_pc(void);
void yield(void);
void swap(uint64_t *prev, uint64_t next);
void sleep(int wait);
void wake_up(int wait);
void exit(void);
void wait(int pid);
int fork(void);
int exec(struct Process *process, char *name);
int grow_process(struct Process *process, int64_t inc);
void boost_ready_processes(void);


#endif
