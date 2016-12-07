# Entry point of kernel
ENTRYPOINT = 0x30400
#ENTRYPOINT = 0x1000
# Entry offset of kernel
ENTRYOFFSET = 0x400

# flags
ASM = nasm
ASMBFLAGS = -I boot/include/
ASMKFLAGS = -I include/ -f elf
CC = gcc
CCFLAGS = -I include/ -c -fno-builtin -fno-stack-protector -m32
LD = ld
LDFLAGS = -melf_i386 -s -Ttext $(ENTRYPOINT)

# targets
COMPILETARGET = boot/boot.bin boot/loader.bin boot/hdboot.bin boot/hdldr.bin kernel.bin
OBJS = kernel/kernel.o kernel/syscall.o lib/klib.o lib/kliba.o lib/string.o lib/misc.o kernel/start.o kernel/protect.o kernel/proc.o kernel/io.o kernel/tty.o kernel/systask.o kernel/main.o kernel/global.o kernel/printf.o kernel/hd.o fs/main.o fs/open_close.o fs/read_write.o fs/unlink.o fs/misc.o mm/main.o lib/fork.o lib/exec.o
LOBJS = kernel/kernel2.o kernel/syscall.o lib/klib.o lib/kliba.o lib/string.o lib/misc.o kernel/start.o kernel/protect.o kernel/proc.o kernel/io.o kernel/tty.o kernel/systask.o kernel/main.o kernel/global.o kernel/printf.o kernel/hd.o fs/main.o fs/open_close.o fs/read_write.o fs/unlink.o fs/misc.o mm/main.o lib/fork.o lib/exec.o

LIB		= lib/hhx008crt.a

.PHONY = compile clean all buildimg everything

compile: $(OBJS) $(COMPILETARGET)

clean : 
	rm -f $(COMPILETARGET) $(OBJS) kernel/kernel2.o

all : clean compile

buildimg :
	dd if=boot/hdboot.bin of=80m.img bs=1 count=446 conv=notrunc
	dd if=boot/hdboot.bin of=80m.img seek=510 skip=510 bs=1 count=2 conv=notrunc
	dd if=boot/boot.bin of=hhx008.img bs=512 count=1 conv=notrunc
	sudo mount -o loop hhx008.img /mnt/
	sudo cp -fv boot/loader.bin /mnt/
	sudo cp -fv kernel.bin /mnt/
	sudo umount /mnt

everything : compile lib buildimg

lib : $(LIB)

boot/boot.bin : boot/boot.asm boot/include/fat12hdr.inc boot/include/load.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

boot/loader.bin : boot/loader.asm boot/include/fat12hdr.inc boot/include/load.inc boot/include/pm.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

boot/hdboot.bin : boot/hdboot.asm boot/include/fat12hdr.inc boot/include/load.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

boot/hdldr.bin : boot/hdldr.asm boot/include/fat12hdr.inc boot/include/load.inc boot/include/pm.inc
	$(ASM) $(ASMBFLAGS) -o $@ $<

kernel.bin : $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

kernel/kernel.o : kernel/kernel.asm include/sconst.inc
	$(ASM) $(ASMKFLAGS) -o $@ $<

kernel/kernel2.o : kernel/kernel2.asm include/sconst.inc
	$(ASM) $(ASMKFLAGS) -o $@ $<

kernel/syscall.o : kernel/syscall.asm include/sconst.inc
	$(ASM) $(ASMKFLAGS) -o $@ $<

kernel/start.o : kernel/start.c include/const.h include/proto.h include/global.h include/protect.h include/proc.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/main.o : kernel/main.c include/const.h include/proto.h include/global.h include/proc.h include/protect.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/protect.o : kernel/protect.c include/const.h include/proto.h include/global.h include/protect.h include/proc.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/proc.o : kernel/proc.c include/const.h include/proto.h include/global.h include/protect.h include/proc.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/io.o : kernel/io.c include/const.h include/proto.h include/global.h include/protect.h include/proc.h include/keymap.h include/io.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/tty.o : kernel/tty.c include/const.h include/proto.h include/global.h include/protect.h include/proc.h include/io.h include/tty.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/printf.o : kernel/printf.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/systask.o : kernel/systask.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/hd.o : kernel/hd.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h
	$(CC) $(CCFLAGS) -o $@ $<

fs/main.o : fs/main.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h include/hd.h
	$(CC) $(CCFLAGS) -o $@ $<

fs/open_close.o : fs/open_close.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h include/hd.h
	$(CC) $(CCFLAGS) -o $@ $<

fs/read_write.o : fs/read_write.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h include/hd.h
	$(CC) $(CCFLAGS) -o $@ $<

fs/unlink.o : fs/unlink.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h include/hd.h
	$(CC) $(CCFLAGS) -o $@ $<

fs/misc.o : fs/misc.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h include/hd.h
	$(CC) $(CCFLAGS) -o $@ $<

mm/main.o : mm/main.c include/const.h include/proc.h include/protect.h include/proto.h include/global.h
	$(CC) $(CCFLAGS) -o $@ $<

kernel/global.o : kernel/global.c include/const.h include/proto.h include/global.h include/protect.h include/proc.h
	$(CC) $(CCFLAGS) -o $@ $<

lib/fork.o : lib/fork.c include/const.h include/protect.h include/proc.h include/proto.h include/global.h
	$(CC) $(CCFLAGS) -o $@ $<

lib/exec.o : lib/exec.c include/const.h include/protect.h include/proc.h include/proto.h include/elf.h include/global.h
	$(CC) $(CCFLAGS) -o $@ $<

lib/misc.o : lib/misc.c include/const.h include/proto.h
	$(CC) $(CCFLAGS) -o $@ $<

lib/kliba.o : lib/kliba.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

lib/klib.o : lib/klib.c include/const.h include/proto.h
	$(CC) $(CCFLAGS) -o $@ $<

lib/string.o : lib/string.asm
	$(ASM) $(ASMKFLAGS) -o $@ $<

$(LIB) : $(LOBJS)
	$(AR) $(ARFLAGS) $@ $^

