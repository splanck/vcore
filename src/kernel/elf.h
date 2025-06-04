#ifndef _ELF_H_
#define _ELF_H_

#include <stdint.h>
#include <stdbool.h>

struct Process;

bool load_elf(struct Process *proc, void *image);

#endif
