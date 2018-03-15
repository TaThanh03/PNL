.PHONY: all 
#.SECONDARY:

ifneq ($(KERNELRELEASE),)

  #obj-m += helloWorld.o
  #obj-m += change_uname.o
  #bj-m += show_sb.o
  obj-m += update_sb.o
  

else
#il faut remplacer $(shell uname -r) par le chemin ver kernel
  KERNELDIR ?= ~/workplace/PNL/linux-4.9.86/
  PWD := $(shell pwd)

all :
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

endif

