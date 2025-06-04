#include "syscall.h"
#include "print.h"
#include "process.h"
#include "keyboard.h"
#include "memory.h"
#include "debug.h"
#include "stddef.h"
#include "file.h"
#include "net/socket.h"

static SYSTEMCALL system_calls[28];

static int sys_sbrk(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    struct Process *proc = pc->current_process;
    int64_t inc = argptr[0];
    uint64_t old = proc->brk;
    if (grow_process(proc, inc) < 0)
        return -1;
    return old;
}

static int sys_write(int64_t *argptr)
{    
    write_screen((char*)argptr[0], (int)argptr[1], 0xf);  
    return (int)argptr[1];
}

static int sys_sleep(int64_t* argptr)
{
    uint64_t old_ticks; 
    uint64_t ticks;
    uint64_t sleep_ticks = argptr[0];

    ticks = get_ticks();
    old_ticks = ticks;

    while (ticks - old_ticks < sleep_ticks) {
       sleep(-1);
       ticks = get_ticks();
    }

    return 0;
}

static int sys_exit(int64_t *argptr)
{
    exit();
    return 0;
}

static int sys_wait(int64_t *argptr)
{
    wait(argptr[0]);
    return 0;
}

static int sys_keyboard_read(int64_t *argptr)
{
    return read_key_buffer();
}

static int sys_get_total_memory(int64_t *argptr)
{
    return get_total_memory();
}

static int sys_open_file(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    return open_file(pc->current_process, (char*)argptr[0]);
}

static int sys_read_file(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    return read_file(pc->current_process, argptr[0], (void*)argptr[1], argptr[2]);
}

static int sys_close_file(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    close_file(pc->current_process, argptr[0]);

    return 0;
}

static int sys_get_file_size(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();  
    return get_file_size(pc->current_process, argptr[0]);
}

static int sys_fork(int64_t *argptr)
{
    return fork();
}

static int sys_exec(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    struct Process *process = pc->current_process; 

    return exec(process, (char*)argptr[0]);
}

static int sys_read_root_directory(int64_t *argptr)
{
    return read_root_directory((char*)argptr[0]);
}

static int sys_create_file(int64_t *argptr)
{
    return create_file((char*)argptr[0]);
}

static int sys_write_file(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    return write_file(pc->current_process, argptr[0], (void*)argptr[1], argptr[2]);
}

static int sys_delete_file(int64_t *argptr)
{
    return delete_file((char*)argptr[0]);
}

static int sys_mkdir(int64_t *argptr)
{
    return mkdir((char*)argptr[0]);
}

static int sys_opendir(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    return opendir(pc->current_process, (char*)argptr[0]);
}

static int sys_readdir(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    return readdir(pc->current_process, argptr[0], (struct DirEntry*)argptr[1]);
}

static int sys_rmdir(int64_t *argptr)
{
    return rmdir((char*)argptr[0]);
}

static int sys_set_priority(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    set_process_priority(pc->current_process, argptr[0]);
    return 0;
}

static int sys_get_priority(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    return pc->current_process->priority;
}

static int sys_get_runtime(int64_t *argptr)
{
    struct ProcessControl *pc = get_pc();
    return (int)pc->current_process->runtime;
}

static int sys_get_sched_info(int64_t *argptr)
{
    struct SchedInfo info;
    struct ProcessControl *pc = get_pc();
    struct Process *p = pc->current_process;
    info.pid = p->pid;
    info.priority = p->priority;
    info.runtime = p->runtime;
    info.time_slice = p->time_slice;
    memcpy((void*)argptr[0], &info, sizeof(info));
    return 0;
}

static int sys_socket(int64_t *argptr)
{
    return socket_create((int)argptr[0]);
}

static int sys_sendto(int64_t *argptr)
{
    return socket_send((int)argptr[0], (const void*)argptr[1], (int)argptr[2]);
}

static int sys_recvfrom(int64_t *argptr)
{
    return socket_recv((int)argptr[0], (void*)argptr[1], (int)argptr[2]);
}

void init_system_call(void)
{
    system_calls[0] = sys_write;
    system_calls[1] = sys_sleep;
    system_calls[2] = sys_exit;
    system_calls[3] = sys_wait;
    system_calls[4] = sys_keyboard_read;
    system_calls[5] = sys_get_total_memory;
    system_calls[6] = sys_open_file;
    system_calls[7] = sys_read_file;  
    system_calls[8] = sys_get_file_size;
    system_calls[9] = sys_close_file; 
    system_calls[10] = sys_fork; 
    system_calls[11] = sys_exec;
    system_calls[12] = sys_read_root_directory;
    system_calls[13] = sys_sbrk;
    system_calls[14] = sys_create_file;
    system_calls[15] = sys_write_file;
    system_calls[16] = sys_delete_file;
    system_calls[17] = sys_socket;
    system_calls[18] = sys_sendto;
    system_calls[19] = sys_recvfrom;
    system_calls[20] = sys_set_priority;
    system_calls[21] = sys_get_priority;
    system_calls[22] = sys_get_runtime;
    system_calls[23] = sys_mkdir;
    system_calls[24] = sys_opendir;
    system_calls[25] = sys_readdir;
    system_calls[26] = sys_rmdir;
    system_calls[27] = sys_get_sched_info;
}

void system_call(struct TrapFrame *tf)
{
    int64_t i = tf->rax;
    int64_t param_count = tf->rdi;
    int64_t *argptr = (int64_t*)tf->rsi;

    if (param_count < 0 || i > 27 || i < 0) {
        tf->rax = -1;
        return;
    }
    
    ASSERT(system_calls[i] != NULL);
    tf->rax = system_calls[i](argptr);
}
