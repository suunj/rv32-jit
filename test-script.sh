clang \
    --target=riscv32-unknown-elf \
    -march=rv32i \
    -mabi=ilp32 \
    -nostdlib \
    -T link.ld \
    test.S \
    -o test.elf

riscv64-buildroot-linux-gnu-objdump -d test.elf
