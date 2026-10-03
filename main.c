
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <elf.h>

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

int validate_magic(const Elf32_Ehdr *ehdr)
{
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return -1;
    }

    return 0;
}

int validate_elf32(const Elf32_Ehdr *ehdr)
{
    return ehdr->e_ident[EI_CLASS] == ELFCLASS32 ? 0 : -1;
}

int validate_little_endian(const Elf32_Ehdr *ehdr)
{
    return ehdr->e_ident[EI_DATA] == ELFDATA2LSB ? 0 : -1;
}

int validate_riscv(const Elf32_Ehdr *ehdr)
{
    return ehdr->e_machine == EM_RISCV ? 0 : -1;
}

int validate_exec(const Elf32_Ehdr *ehdr)
{
    return ehdr->e_type == ET_EXEC ? 0 : -1;
}

int main(int argc, char **argv)
{
    FILE *fp;
    const char *path;
    Elf32_Ehdr ehdr;
    Machine m = {0};
    uint8_t ram_data[RAM_SIZE] = {0};

    if (argc != 2) {
        fprintf(stderr, "usage: %s <elf>\n", argv[0]);
        return 1;
    }

    path = argv[1];

    m.ram.data = ram_data;
    m.ram.size = RAM_SIZE;
    m.ram.base = RAM_BASE;

    fp = fopen(path, "rb");
    if (!fp)
        return -1;

    /* 1. ELF Header */
    if (fread(&ehdr, sizeof(ehdr), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (validate_magic(&ehdr) != 0) {
        fprintf(stderr, "invalid ELF magic\n");
        fclose(fp);
        return 1;
    }

    if (validate_elf32(&ehdr) != 0) {
        fprintf(stderr, "not ELF32\n");
        fclose(fp);
        return 1;
    }

    if (validate_little_endian(&ehdr) != 0) {
        fprintf(stderr, "invalid ELF endian\n");
        fclose(fp);
        return 1;
    }

    if (validate_riscv(&ehdr) != 0) {
        fprintf(stderr, "not support architecture\n");
        fclose(fp);
        return 1;
    }

    if (validate_exec(&ehdr) != 0) {
        fprintf(stderr, "not executable ELF\n");
        fclose(fp);
        return 1;
    }

    /* 2. Program Headers */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        uint32_t off;

        if (fseek(fp, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) != 0) {
            perror("fseek");
            fclose(fp);
            return 1;
        }

        if (fread(&phdr, sizeof(phdr), 1, fp) != 1) {
            fprintf(stderr, "failed to read program header\n");
            fclose(fp);
            return 1;
        }

        if (phdr.p_type != PT_LOAD)
            continue;

        printf("PT_LOAD[%d]\n", i);
        printf("  offset = 0x%08x\n", phdr.p_offset);
        printf("  vaddr  = 0x%08x\n", phdr.p_vaddr);
        printf("  filesz = 0x%08x\n", phdr.p_filesz);
        printf("  memsz  = 0x%08x\n", phdr.p_memsz);
        printf("  flags  = 0x%08x\n", phdr.p_flags);

        // guest virtual address -> host RAM-array offset
        if (phdr.p_memsz < phdr.p_filesz) {
            fprintf(stderr, "invalid segment size\n");
            fclose(fp);
            return 1;
        }

        if (phdr.p_vaddr < m.ram.base) {
            fprintf(stderr, "segment below RAM\n");
            fclose(fp);
            return 1;
        }

        off = phdr.p_vaddr - m.ram.base;

        if ((uint64_t)off + phdr.p_memsz > m.ram.size) {
            fprintf(stderr, "segment outside RAM\n");
            fclose(fp);
            return 1;
        }

        if (fseek(fp, phdr.p_offset, SEEK_SET) != 0) {
            perror("fseek");
            fclose(fp);
            return 1;
        }

        if (fread(m.ram.data + off,
                    1,
                    phdr.p_filesz,
                    fp) != phdr.p_filesz) {
            fprintf(stderr, "failed to read segment\n");
            fclose(fp);
            return 1;
        }
        // zero-filled
        if (phdr.p_memsz > phdr.p_filesz) {
            memset(m.ram.data + off + phdr.p_filesz, 0,
                   phdr.p_memsz - phdr.p_filesz);
        }
        /* ... */
    }

    if (ehdr.e_entry < m.ram.base ||
            (uint64_t)ehdr.e_entry >= (uint64_t)m.ram.base + m.ram.size) {
        fprintf(stderr, "entry point outside RAM\n");
        fclose(fp);
        return 1;
    }
    m.cpu.pc = ehdr.e_entry;

    printf("entry = 0x%08x\n", m.cpu.pc);

    fclose(fp);
    return 0;
}
