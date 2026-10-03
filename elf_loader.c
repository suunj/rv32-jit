#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <elf.h>

#include "machine.h"
#include "elf_loader.h"

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

static int elf_validate(const Elf32_Ehdr *ehdr)
{
    if (validate_magic(ehdr) != 0) {
        fprintf(stderr, "invalid ELF magic\n");
        return -1;
    }

    if (validate_elf32(ehdr) != 0) {
        fprintf(stderr, "not ELF32\n");
        return -1;
    }

    if (validate_little_endian(ehdr) != 0) {
        fprintf(stderr, "invalid ELF endian\n");
        return -1;
    }

    if (validate_riscv(ehdr) != 0) {
        fprintf(stderr, "unsupported architecture\n");
        return -1;
    }

    if (validate_exec(ehdr) != 0) {
        fprintf(stderr, "not executable ELF\n");
        return -1;
    }

    return 0;
}

static int load_segment(Machine *m, FILE *fp, const Elf32_Phdr *phdr)
{
    uint32_t off;

    if (phdr->p_memsz < phdr->p_filesz) {
        fprintf(stderr, "invalid segment size\n");
        return -1;
    }

    if (phdr->p_vaddr < m->ram.base) {
        fprintf(stderr, "segment below RAM\n");
        return -1;
    }

    off = phdr->p_vaddr - m->ram.base;

    if ((uint64_t)off + phdr->p_memsz > m->ram.size) {
        fprintf(stderr, "segment outside RAM\n");
        return -1;
    }

    if (fseek(fp, phdr->p_offset, SEEK_SET) != 0) {
        perror("fseek");
        return -1;
    }

    if (fread(m->ram.data + off, 1, phdr->p_filesz, fp)
        != phdr->p_filesz) {
        fprintf(stderr, "failed to read segment\n");
        return -1;
    }

    if (phdr->p_memsz > phdr->p_filesz) {
        memset(m->ram.data + off + phdr->p_filesz,
               0,
               phdr->p_memsz - phdr->p_filesz);
    }

    return 0;
}

int elf_load(Machine *m, const char *path)
{
    FILE *fp;
    Elf32_Ehdr ehdr;

    fp = fopen(path, "rb");
    if (!fp)
        return -1;

    if (fread(&ehdr, sizeof(ehdr), 1, fp) != 1)
        goto fail;

    if (elf_validate(&ehdr) != 0)
        goto fail;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;

        if (fseek(fp, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET) != 0)
            goto fail;

        if (fread(&phdr, sizeof(phdr), 1, fp) != 1)
            goto fail;

        if (phdr.p_type != PT_LOAD)
            continue;

        printf("PT_LOAD[%d]\n", i);
        printf("  offset = 0x%08x\n", phdr.p_offset);
        printf("  vaddr  = 0x%08x\n", phdr.p_vaddr);
        printf("  filesz = 0x%08x\n", phdr.p_filesz);
        printf("  memsz  = 0x%08x\n", phdr.p_memsz);
        printf("  flags  = 0x%08x\n", phdr.p_flags);

        if (load_segment(m, fp, &phdr) != 0)
            goto fail;
    }

    if (ehdr.e_entry < m->ram.base ||
        (uint64_t)ehdr.e_entry >= (uint64_t)m->ram.base + m->ram.size) {
        fprintf(stderr, "entry point outside RAM\n");
        goto fail;
    }

    m->cpu.pc = ehdr.e_entry;

    fclose(fp);
    return 0;

fail:
    fclose(fp);
    return -1;
}


