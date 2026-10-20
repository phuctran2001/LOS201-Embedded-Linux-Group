obj-m += sms_sensor_driver.o
KDIR ?= $(HOME)/LOS201/asgn02/kernel

.PHONY: arm clean

arm:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KCFLAGS=-march=armv7-a modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean
