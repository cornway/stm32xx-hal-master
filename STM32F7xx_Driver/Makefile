PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)

include $(TOP)/configs/$(PLATFORM)/boot.mk
include $(TOP)/.config


CCFLAGS := $(CCFLAGS_MK)
CCDEFS := $(CCDEFS_MK)
CCINC := $(CCINC_MK)
CCINC += -I$(TOP)/main/Inc \
		-I$(TOP)/common/Utilities/JPEG \
		-I$(TOP)/ulib/pub \
		-I$(TOP)/ulib/arch \
		$(HALINC_MK)

bsp :
	mkdir -p ./.output/bsp/Components
	mkdir -p ./.output/bsp/src
	mkdir -p ./.output/bsp/obj

	cp -r ./BSP/STM32F769I-Discovery/* ./.output/bsp/src
	cp -r ./BSP/Components/Common ./.output/bsp/Components

ifeq ($(HAVE_HDMI), 1)
	cp -r ./BSP/Components/adv7533 ./.output/bsp/Components
endif

ifeq ($(HAVE_AUDIO), 1)
	cp -r ./BSP/Components/wm8994 ./.output/bsp/Components
endif

	cp Makefile ./.output/bsp

	$(MAKE) _bsp TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./.output/bsp

	mv ./.output/bsp/*.o ./.output/bsp/obj/

hal :
	mkdir -p ./.output/hal/src
	mkdir -p ./.output/hal/obj

	cp -r ./Src/* ./.output/hal/src
	cp Makefile ./.output/hal/src

	$(MAKE) _hal TOP=$(TOP) -C ./.output/hal/src

	mv ./.output/hal/src/*.o ./.output/hal/obj/

_hal :
	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./*.c

_bsp :
	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./Components/*/*.c
	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./src/*.c

clean :
	rm -rf ./.output

