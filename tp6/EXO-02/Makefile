.PHONY: all 

ifneq ($(KERNELRELEASE),)

obj-m := helloioctl.o
CFLAGS_helloioctl.o := -DDEBUG

else

  KERNELDIR ?= ~/workplace/PNL/linux-4.9.86/
  PWD := $(shell pwd)

all:
	make -C $(KERNELDIR) M=$$PWD modules

clean:
	make -C $(KERNELDIR) M=$$PWD clean

endif
