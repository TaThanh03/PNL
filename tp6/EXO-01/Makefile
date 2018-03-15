.PHONY: all 

ifneq ($(KERNELRELEASE),)

obj-m := hellosysfs.o
CFLAGS_kcpustat.o := -DDEBUG

else

  KERNELDIR ?= ~/workplace/PNL/linux-4.9.86/
  PWD := $(shell pwd)

all:
	make -C $(KERNELDIR) M=$$PWD modules

clean:
	make -C $(KERNELDIR) M=$$PWD clean

endif
