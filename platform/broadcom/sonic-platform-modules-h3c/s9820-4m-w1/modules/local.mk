#KVER :=4.9.0-8-amd64

KVER :=$(shell uname -r)

obj-m:=bsp_base.o syseeprom.o psu.o fan.o sensor.o cpld.o xcvr.o sysled.o slot.o drv_cpld.o

KDIR :=/lib/modules/$(KVER)/build
PWD := $(shell pwd)


all:
	make -C $(KDIR) $(CFLAGS)  M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *.so
