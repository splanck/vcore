#include "process.h"
#include "trap.h"
#include "memory.h"
#include "print.h"
#include "lib.h"
#include "debug.h"
#include "cpu.h"
#include "elf.h"

extern struct TSS Tss;
static struct Process process_table[NUM_PROC];
static int pid_num = 1;

static void set_tss(struct Process *proc)
{
    Tss.rsp0 = proc->stack + STACK_SIZE;    
}

static struct Process* find_unused_process(void)
{
    struct Process *process = NULL;

    for (int i = 0; i < NUM_PROC; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            process = &process_table[i];
            break;
        }
    }

    return process;
}

static struct Process* alloc_new_process(void)
{
    uint64_t stack_top;
    struct Process *proc;

    proc = find_unused_process();
    if (proc == NULL) {
        return NULL;
    }

    proc->state = PROC_INIT;
    proc->pid = pid_num++;
    proc->priority = 1;
    proc->cpu_id = cpu_current()->id;

    proc->stack = (uint64_t)kalloc();
    if (proc->stack == 0) {
        return NULL;
    }

    memset((void*)proc->stack, 0, PAGE_SIZE);   
    stack_top = proc->stack + STACK_SIZE;

    proc->context = stack_top - sizeof(struct TrapFrame) - 7*8;   
    *(uint64_t*)(proc->context + 6*8) = (uint64_t)TrapReturn;

    proc->tf = (struct TrapFrame*)(stack_top - sizeof(struct TrapFrame)); 
    proc->tf->cs = 0x10|3;
    proc->tf->rip = 0x400000;
    proc->tf->ss = 0x18|3;
    proc->tf->rsp = 0x400000 + PAGE_SIZE;
    proc->tf->rflags = 0x202;
    proc->brk = 0x400000 + PAGE_SIZE;
    
    proc->page_map = setup_kvm();
    if (proc->page_map == 0) {
        kfree(proc->stack);
        memset(proc, 0, sizeof(struct Process));
        return NULL;
    }

    return proc;    
}

struct ProcessControl* get_pc(void)
{
    struct CPU *cpu = cpu_current();
    return &cpu->pc;
}

static void init_idle_process(void)
{
    struct Process *process;
    struct ProcessControl *process_control;

    process = find_unused_process();
    ASSERT(process == &process_table[0]);

    process->pid = 0;
    process->page_map = P2V(read_cr3());
    process->state = PROC_RUNNING;
    process->priority = MAX_PRIORITY - 1;
    process->cpu_id = 0;
    process->brk = 0;

    process_control = get_pc();
    process_control->current_process = process;
}

static void init_user_process(void)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;

    process_control = get_pc();

    process = alloc_new_process();
    ASSERT(process != NULL);

    list = &process_control->ready_list[process->priority];

    ASSERT(load_elf(process, (void*)P2V(0x30000)) == true);

    process->state = PROC_READY;
    append_list_tail(list, (struct List*)process);
}

void init_process(void)
{
    init_idle_process();
    init_user_process();
}

void set_process_priority(struct Process *proc, int priority)
{
    if (priority < 0)
        priority = 0;
    if (priority >= MAX_PRIORITY)
        priority = MAX_PRIORITY - 1;
    proc->priority = priority;
}

static void switch_process(struct Process *prev, struct Process *current)
{
    set_tss(current);
    switch_vm(current->page_map);
    swap(&prev->context, current->context);
}

static void schedule(void)
{
    struct Process *prev_proc;
    struct Process *current_proc;
    struct ProcessControl *process_control;
    struct HeadList *list;

    process_control = get_pc();
    prev_proc = process_control->current_process;
    current_proc = NULL;

    for (int pr = 0; pr < MAX_PRIORITY; pr++) {
        list = &process_control->ready_list[pr];
        if (!is_list_empty(list)) {
            current_proc = (struct Process*)remove_list_head(list);
            break;
        }
    }

    /* simple load balancing: steal from other CPUs if idle */
    if (current_proc == NULL) {
        for (int cpu = 0; cpu < cpu_count && current_proc == NULL; cpu++) {
            struct ProcessControl *other = &cpus[cpu].pc;
            if (other == process_control)
                continue;
            for (int pr = 0; pr < MAX_PRIORITY; pr++) {
                list = &other->ready_list[pr];
                if (!is_list_empty(list)) {
                    current_proc = (struct Process*)remove_list_head(list);
                    break;
                }
            }
        }
    }

    if (current_proc == NULL) {
        ASSERT(process_control->current_process->pid != 0);
        current_proc = &process_table[0];
    }

    current_proc->state = PROC_RUNNING;
    current_proc->cpu_id = cpu_current()->id;
    process_control->current_process = current_proc;

    switch_process(prev_proc, current_proc);
}

void yield(void)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;

    process_control = get_pc();
    process = process_control->current_process;
    process->state = PROC_READY;

    if (process->pid != 0) {
        list = &process_control->ready_list[process->priority];
        append_list_tail(list, (struct List*)process);
    }

    schedule();
    reschedule_other_cpus();
}

void sleep(int wait)
{
    struct ProcessControl *process_control;
    struct Process *process;
    
    process_control = get_pc();
    process = process_control->current_process;
    process->state = PROC_SLEEP;
    process->wait = wait;

    append_list_tail(&process_control->wait_list, (struct List*)process);
    schedule();
}

void wake_up(int wait)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *ready_list;
    struct HeadList *wait_list;

    process_control = get_pc();
    wait_list = &process_control->wait_list;
    process = (struct Process*)remove_list(wait_list, wait);

    while (process != NULL) {
        process->state = PROC_READY;
        ready_list = &process_control->ready_list[process->priority];
        append_list_tail(ready_list, (struct List*)process);
        process = (struct Process*)remove_list(wait_list, wait);
    }
    reschedule_other_cpus();
}

void exit(void)
{
    struct ProcessControl *process_control;
    struct Process* process;
    struct HeadList *list;

    process_control = get_pc();
    process = process_control->current_process;
    process->state = PROC_KILLED;
    process->wait = process->pid;

    list = &process_control->kill_list;
    append_list_tail(list, (struct List*)process);

    wake_up(-3);
    schedule();
    reschedule_other_cpus();
}

void wait(int pid)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;

    process_control = get_pc();
    list = &process_control->kill_list;

    while (1) {
        if (!is_list_empty(list)) {
            process = (struct Process*)remove_list(list, pid); 
            if (process != NULL) {
                ASSERT(process->state == PROC_KILLED);
                kfree(process->stack);
                free_vm(process->page_map, process->brk - 0x400000);

                for (int i = 0; i < 100; i++) {
                    if (process->file[i] != NULL) {
                        process->file[i]->fcb->count--;
                        process->file[i]->count--;

                        if (process->file[i]->count == 0) {
                            process->file[i]->fcb = NULL;
                        }
                    }
                }
                memset(process, 0, sizeof(struct Process));
                break;
            }   
        }
       
        sleep(-3);     
    }
}

int fork(void)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct Process *current_process;
    struct HeadList *list;

    process_control = get_pc();
    current_process = process_control->current_process;

    process = alloc_new_process();
    if (process == NULL) {
        ASSERT(0);
        return -1;
    }

    if (share_uvm(process->page_map, current_process->page_map,
                  current_process->brk - 0x400000) == false) {
        ASSERT(0);
        return -1;
    }

    memcpy(process->file, current_process->file, 100 * sizeof(struct FileDesc*));

    for (int i = 0; i < 100; i++) {
        if (process->file[i] != NULL) {
            process->file[i]->count++;
            process->file[i]->fcb->count++;
        }
    }

    memcpy(process->tf, current_process->tf, sizeof(struct TrapFrame));
    process->tf->rax = 0;
    process->brk = current_process->brk;
    process->priority = current_process->priority;
    process->cpu_id = current_process->cpu_id;
    process->state = PROC_READY;
    list = &process_control->ready_list[process->priority];
    append_list_tail(list, (struct List*)process);
    reschedule_other_cpus();

    return process->pid;
}

int exec(struct Process *process, char* name)
{
    int fd;
    uint32_t size;
    void *buf;

    fd = open_file(process, name);
    if (fd == -1)
        exit();

    size = get_file_size(process, fd);
    buf = kmalloc(size);
    if (!buf) {
        close_file(process, fd);
        exit();
    }

    if (read_file(process, fd, buf, size) != size) {
        kmfree(buf);
        close_file(process, fd);
        exit();
    }

    close_file(process, fd);

    memset(process->tf, 0, sizeof(struct TrapFrame));
    process->tf->cs = 0x10|3;
    process->tf->ss = 0x18|3;
    process->tf->rflags = 0x202;

    if (!load_elf(process, buf)) {
        kmfree(buf);
        exit();
    }

    kmfree(buf);
    return 0;
}

int grow_process(struct Process *process, int64_t inc)
{
    if (inc > 0) {
        process->brk += inc;
    } else if (inc < 0) {
        uint64_t dec = -inc;
        if (dec > process->brk - 0x400000)
            dec = process->brk - 0x400000;
        uint64_t new_brk = process->brk - dec;
        for (uint64_t addr = PA_UP(new_brk); addr < PA_UP(process->brk); addr += PAGE_SIZE) {
            free_pages(process->page_map, addr, addr + PAGE_SIZE);
        }
        process->brk = new_brk;
    }

    return 0;
}
