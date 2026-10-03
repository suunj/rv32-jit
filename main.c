#include <stdint.h>
#include <stdio.h>

#include "machine.h"
#include "elf_loader.h"

int main(int argc, char **argv)
{
    Machine m = {0};
    uint8_t ram_data[RAM_SIZE] = {0};

    if (argc != 2) {
        fprintf(stderr, "usage: %s <elf>\n", argv[0]);
        return 1;
    }

    m.ram.data = ram_data;
    m.ram.size = RAM_SIZE;
    m.ram.base = RAM_BASE;

    if (elf_load(&m, argv[1]) != 0) {
        fprintf(stderr, "failed to load ELF\n");
        return 1;
    }

    printf("entry = 0x%08x\n", m.cpu.pc);

    return 0;
}
