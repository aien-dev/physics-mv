# Research build. NVRM_INC points at NVIDIA open-gpu-kernel-modules tag 580.173.02 (MIT).
NVRM_INC ?= $(HOME)/workspace/.ref/ogkm
OUT ?= build
CC ?= gcc
CFLAGS ?= -std=c11 -O1 -Wall -Wextra -D_GNU_SOURCE \
  -I$(NVRM_INC)/src/common/sdk/nvidia/inc -I$(NVRM_INC)/kernel-open/common/inc \
  -I$(NVRM_INC)/kernel-open/nvidia-uvm -I$(NVRM_INC)/src/nvidia/arch/nvalloc/unix/include

all: $(OUT)/sem_test

$(OUT):
	mkdir -p $(OUT)

$(OUT)/sem_test: sem_test.c nvrm.c nvrm.h | $(OUT)
	$(CC) $(CFLAGS) -o $@ sem_test.c nvrm.c

clean:
	rm -rf $(OUT)
