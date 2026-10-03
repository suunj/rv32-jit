CC := gcc

CFLAGS := -Wall -Wextra -O0 -g

TARGET := loader
OBJS := main.o elf_loader.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o: main.c machine.h elf_loader.h
	$(CC) $(CFLAGS) -c main.c

elf_loader.o: elf_loader.c machine.h elf_loader.h
	$(CC) $(CFLAGS) -c elf_loader.c

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean
