CFLAGS= -I src/qoi -I src/mock_libc -g -Isrc/umka/src -m32 -march=i686 -Os -fno-builtin -fcf-protection=none -nostdlib -ffreestanding -mno-red-zone -fno-exceptions
EMU=qemu-system-x86_64

all: pre-build do-build

pre-build: 
	mkdir -p bin
	python3 mkfs.py fs bin/fs.bin

do-build: bin/unity.o bin/umka.o bin/boot.o
	ld -melf_i386 -r -b binary -o bin/fs.o bin/fs.bin
	gcc $(CFLAGS) bin/unity.o bin/umka.o bin/fs.o bin/boot.o -o bin/sys.o -T src/sys/link.ld
	objcopy -O binary bin/sys.o bin/sys.img

run: all
	$(EMU) -no-shutdown -no-reboot -m 128 -drive file=bin/sys.img,format=raw

debug: all
	@$(EMU) -no-shutdown -no-reboot -s -S -m 128 -drive file=bin/sys.img,format=raw &
	sleep 1
	gdb -tui

bin/unity.o: $(wildcard src/sys/*.c) $(wildcard src/mock_libc/*.c)
	gcc $(CFLAGS) -c src/unity.c -o bin/unity.o

bin/umka.o: src/umka.c $(wildcard src/umka/src/*.c)
	gcc $(CFLAGS) -c src/umka.c -o bin/umka.o

bin/boot.o: $(wildcard ./src/sys/*.s)
	nasm -f elf32 src/sys/boot.s -o bin/boot.o

clean:
	rm -f bin/*.o


