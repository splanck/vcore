#include "trap.h"
#include "print.h"
#include "syscall.h"
#include "process.h"
#include "keyboard.h"
#include "debug.h"
#include "memory.h"

static struct IdtPtr idt_pointer;
static struct IdtEntry vectors[256];
static uint64_t ticks;

static void init_idt_entry(struct IdtEntry *entry, uint64_t addr, uint8_t attribute)
{
    entry->low = (uint16_t)addr;
    entry->selector = 8;
    entry->attr = attribute;
    entry->mid = (uint16_t)(addr>>16);
    entry->high = (uint32_t)(addr>>32);
}

void init_idt(void)
{
    init_idt_entry(&vectors[0],(uint64_t)vector0,0x8e);
    init_idt_entry(&vectors[1],(uint64_t)vector1,0x8e);
    init_idt_entry(&vectors[2],(uint64_t)vector2,0x8e);
    init_idt_entry(&vectors[3],(uint64_t)vector3,0x8e);
    init_idt_entry(&vectors[4],(uint64_t)vector4,0x8e);
    init_idt_entry(&vectors[5],(uint64_t)vector5,0x8e);
    init_idt_entry(&vectors[6],(uint64_t)vector6,0x8e);
    init_idt_entry(&vectors[7],(uint64_t)vector7,0x8e);
    init_idt_entry(&vectors[8],(uint64_t)vector8,0x8e);
    init_idt_entry(&vectors[10],(uint64_t)vector10,0x8e);
    init_idt_entry(&vectors[11],(uint64_t)vector11,0x8e);
    init_idt_entry(&vectors[12],(uint64_t)vector12,0x8e);
    init_idt_entry(&vectors[13],(uint64_t)vector13,0x8e);
    init_idt_entry(&vectors[14],(uint64_t)vector14,0x8e);
    init_idt_entry(&vectors[16],(uint64_t)vector16,0x8e);
    init_idt_entry(&vectors[17],(uint64_t)vector17,0x8e);
    init_idt_entry(&vectors[18],(uint64_t)vector18,0x8e);
    init_idt_entry(&vectors[19],(uint64_t)vector19,0x8e);
    init_idt_entry(&vectors[32],(uint64_t)vector32,0x8e);
    init_idt_entry(&vectors[33],(uint64_t)vector33,0x8e);
    init_idt_entry(&vectors[39],(uint64_t)vector39,0x8e);
    init_idt_entry(&vectors[0x80],(uint64_t)sysint,0xee);

    idt_pointer.limit = sizeof(vectors)-1;
    idt_pointer.addr = (uint64_t)vectors;
    load_idt(&idt_pointer);
}

uint64_t get_ticks(void)
{
    return ticks;
}

static void timer_handler(void)
{
    ticks++;
    wake_up(-1);
}

static int handle_page_fault(struct TrapFrame *tf)
{
    uint64_t addr = read_cr2();
    struct ProcessControl *pc = get_pc();
    struct Process *proc = pc->current_process;

    if (addr >= proc->brk)
        grow_process(proc, addr + PAGE_SIZE - proc->brk);

    uint64_t va = PA_DOWN(addr);
    PD pd = find_pdpt_entry(proc->page_map, va, 0, 0);
    if (!pd)
        return -1;
    unsigned int idx = (va >> 21) & 0x1FF;

    if (!(pd[idx] & PTE_P)) {
        void *page = kalloc();
        if (!page)
            return -1;
        memset(page, 0, PAGE_SIZE);
        if (!map_pages(proc->page_map, va, va + PAGE_SIZE, V2P(page), PTE_P|PTE_W|PTE_U)) {
            kfree((uint64_t)page);
            return -1;
        }
        return 0;
    }

    if ((tf->errorcode & 2) && !(pd[idx] & PTE_W)) {
        uint64_t pa = PTE_ADDR(pd[idx]);
        if (page_getref(pa) > 1) {
            void *page = kalloc();
            if (!page)
                return -1;
            memcpy(page, (void*)P2V(pa), PAGE_SIZE);
            page_decref(pa);
            pd[idx] = (PDE)(V2P(page) | PTE_P|PTE_W|PTE_U|PTE_ENTRY);
        } else {
            pd[idx] |= PTE_W;
        }
        return 0;
    }

    return -1;
}

void handler(struct TrapFrame *tf)
{
    unsigned char isr_value;

    switch (tf->trapno) {
        case 32:  
            timer_handler();   
            eoi();
            break;

        case 33:  
            keyboard_handler();   
            eoi();
            break;
            
        case 39:
            isr_value = read_isr();
            if ((isr_value&(1<<7)) != 0) {
                eoi();
            }
            break;

        case 14:
            if (handle_page_fault(tf) < 0) {
                if ((tf->cs & 3) == 3)
                    exit();
                else
                    while (1) {}
            }
            break;

        case 0x80:                   
            system_call(tf);
            break;

        default:
            if ((tf->cs & 3) == 3) {
                printk("Exception is %d\n", tf->trapno);
                exit();
            }
            else {
                while (1) { }
            }
    }

    if (tf->trapno == 32) {       
        yield();
    }
}
