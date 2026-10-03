#ifndef MACHINE_H
#define MACHINE_H

#include <stdint.h>

#define RAM_BASE 0x80000000u
#define RAM_SIZE (1024 * 1024)

typedef struct {
    uint32_t reg[32];
    uint32_t pc;
} CPUState;

typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t base;
} RAM;

typedef struct Machine {
    CPUState cpu;
    RAM ram;
} Machine;

#endif /* MACHINE_H */
