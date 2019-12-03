PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)

include $(TOP)/configs/$(PLATFORM)/boot.mk

CCFLAGS := $(CCFLAGS_MK)
CCDEFS := $(CCDEFS_MK)
CCINC := $(CCINC_MK)
CCINC += -I$(TOP)/main/Inc \
		-I$(TOP)/common/Utilities/JPEG \
		-I$(TOP)/ulib/pub \
		-I$(TOP)/ulib/arch \
		$(HALINC_MK)

f769_bsp :
	mkdir -p ./.output/bsp/src
	mkdir -p ./.output/bsp/obj

	cp -r ./BSP/STM32F769I-Discovery/* ./.output/bsp/src
	cp -r ./BSP/Components ./.output/bsp/
	cp Makefile ./.output/bsp

	$(MAKE) _f769_bsp TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./.output/bsp

	mv ./.output/bsp/*.o ./.output/bsp/obj/

f769_hal :
	mkdir -p ./.output/hal/src
	mkdir -p ./.output/hal/obj

	cp -r ./Src/* ./.output/hal/src
	cp Makefile ./.output/hal/src

	$(MAKE) _f769_hal TOP=$(TOP) -C ./.output/hal/src

	mv ./.output/hal/src/*.o ./.output/hal/obj/

_f769_hal :
	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./*.c

_f769_bsp :
	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./Components/*/*.c
	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./src/*.c

clean :
	rm -rf ./.output

