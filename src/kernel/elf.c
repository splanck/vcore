#include "elf.h"
#include "process.h"
#include "memory.h"
#include "lib.h"
#include "debug.h"
#include "cpu.h"

struct Elf64_Ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed));

#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4

static inline uint64_t align_up(uint64_t val)
{
    return (val + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

bool load_elf(struct Process *proc, void *image)
{
    struct Elf64_Ehdr *eh = (struct Elf64_Ehdr*)image;
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        return false;

    /* free old memory */
    if (proc->brk > 0x400000)
        free_vm(proc->page_map, proc->brk - 0x400000);

    uint64_t max_end = 0;
    struct Elf64_Phdr *ph = (struct Elf64_Phdr*)((char*)image + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; i++, ph++) {
        if (ph->p_type != PT_LOAD)
            continue;
        uint64_t start = PA_DOWN(ph->p_vaddr);
        uint64_t end = PA_UP(ph->p_vaddr + ph->p_memsz);
        for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
            void *page = kalloc();
            if (!page)
                return false;
            memset(page, 0, PAGE_SIZE);
            uint32_t attr = PTE_P | PTE_U;
            if (ph->p_flags & PF_W)
                attr |= PTE_W;
            if (!map_pages(proc->page_map, addr, addr + PAGE_SIZE,
                            V2P(page), attr))
                return false;
        }
        if (ph->p_vaddr + ph->p_memsz > max_end)
            max_end = ph->p_vaddr + ph->p_memsz;
    }

    uint64_t old = read_cr3();
    switch_vm(proc->page_map);
    ph = (struct Elf64_Phdr*)((char*)image + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; i++, ph++) {
        if (ph->p_type != PT_LOAD)
            continue;
        memcpy((void*)ph->p_vaddr, (char*)image + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset((void*)(ph->p_vaddr + ph->p_filesz), 0,
                   ph->p_memsz - ph->p_filesz);
    }
    switch_vm(P2V(old));

    /* setup stack */
    uint64_t stack_base = align_up(max_end);
    void *page = kalloc();
    if (!page)
        return false;
    memset(page, 0, PAGE_SIZE);
    if (!map_pages(proc->page_map, stack_base, stack_base + PAGE_SIZE,
                   V2P(page), PTE_P|PTE_W|PTE_U))
        return false;

    proc->tf->rip = eh->e_entry;
    proc->tf->rsp = stack_base + PAGE_SIZE;
    proc->brk = stack_base + PAGE_SIZE;

    return true;
}

