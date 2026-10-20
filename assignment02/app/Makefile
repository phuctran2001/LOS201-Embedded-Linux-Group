HOST_CC := gcc
ARM_CC := arm-linux-gnueabihf-gcc
SOURCES := $(wildcard *.c)
CFLAGS := -std=c11 -O2 -g -Wall -Wextra -I.
LIBS := -pthread -lrt -lm

.PHONY: arm host
arm:
	mkdir -p build
	$(ARM_CC) $(CFLAGS) -static $(SOURCES) -o build/sms_app_arm $(LIBS)

host:
	mkdir -p build
	$(HOST_CC) $(CFLAGS) $(SOURCES) -o build/sms_app_x86 $(LIBS)
