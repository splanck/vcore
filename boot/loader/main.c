#include "print.h"
#include "file.h"
#include "debug.h"
#include "stdint.h"
#include "lib.h"

extern uint64_t kernel_entry;

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

static uint64_t load_kernel(char *name)
{
    ASSERT(load_file(name, 0x200000) == 0);
    struct Elf64_Ehdr *eh = (struct Elf64_Ehdr*)0x200000;
    struct Elf64_Phdr *ph = (struct Elf64_Phdr*)((char*)eh + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; i++, ph++) {
        if (ph->p_type != PT_LOAD)
            continue;
        memcpy((void*)ph->p_paddr, (void*)((char*)eh + ph->p_offset), ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset((void*)(ph->p_paddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
    }
    return eh->e_entry;
}

void EMain(void)
{
   init_fs();
   kernel_entry = load_kernel("KERNEL.ELF");
   ASSERT(load_file("USER.ELF", 0x30000) == 0);
}
