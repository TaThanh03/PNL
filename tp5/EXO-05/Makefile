.PHONY: all 
#.SECONDARY:

ifneq ($(KERNELRELEASE),)

obj-m := kcpustat.o

CFLAGS_kcpustat.o := -DDEBUG

else
	
  KERNELDIR ?= ~/workplace/PNL/linux-4.9.86/
  PWD := $(shell pwd)

all :
	$(info obj-m : $(obj-m))
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

endif
