# DSO-TRON Makefile for x86_64
CC      = gcc
AS      = as
LD      = ld.lld
CFLAGS  = -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
          -mno-red-zone -m64 -mgeneral-regs-only \
          -Wall -Wextra -Werror -O2 \
          -Iinclude -Ikernel -Isysdepend/x86_64
ASFLAGS = -I sysdepend/x86_64
LDFLAGS = -T sysdepend/x86_64/linker.ld -n -static

OBJDIR  = build
SRCS_C  = kernel/dso_common.c kernel/dso_tsk.c kernel/dso_dtq.c \
          kernel/dso_init.c kernel/dso_stubs.c kernel/main.c \
           sysdepend/x86_64/vga.c sysdepend/x86_64/pit.c \
           sysdepend/x86_64/pic.c sysdepend/x86_64/gdt.c \
           sysdepend/x86_64/idt.c sysdepend/x86_64/serial.c
SRCS_S  = sysdepend/x86_64/boot.S sysdepend/x86_64/ctx_switch.S
OBJS_C  = $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS_C))
OBJS_S  = $(patsubst %.S,$(OBJDIR)/%.o,$(SRCS_S))
OBJS    = $(OBJS_C) $(OBJS_S)

.PHONY: all iso run clean

all: $(OBJDIR)/dso-tron.bin

$(OBJDIR)/%.o: %.c $(OBJDIR)/.dirstamp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.S $(OBJDIR)/.dirstamp
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(OBJDIR)/.dirstamp:
	@mkdir -p $(OBJDIR)
	@touch $@

$(OBJDIR)/dso-tron.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(OBJDIR)/dso-tron.bin: $(OBJDIR)/dso-tron.elf
	cp $< $@

iso: $(OBJDIR)/dso-tron.bin
	mkdir -p iso/boot/grub
	cp $< iso/boot/dso-tron.bin
	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo '' >> iso/boot/grub/grub.cfg
	echo 'menuentry "DSO-TRON" {' >> iso/boot/grub/grub.cfg
	echo '  multiboot2 /boot/dso-tron.bin' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o dso-tron.iso iso

run: iso
	qemu-system-x86_64 -cdrom dso-tron.iso -nographic -no-reboot -serial mon:stdio

clean:
	rm -rf $(OBJDIR) iso dso-tron.iso
