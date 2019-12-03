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

hal :
	$(MAKE) f769_hal TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./$(ARCHNAME_MK)_Driver

bsp :
	$(MAKE) f769_bsp TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./$(ARCHNAME_MK)_Driver

CCINC_COM := -I$(TOP)/common/int

com :
	mkdir -p ./.output/lib
	mkdir -p ./.output/obj

	mkdir -p  ./hal/.output/obj
	mkdir -p  ./hal/.output/lib

	cp -r ./hal/*.c ./hal/.output

	cp ./Makefile ./hal/.output

	$(MAKE) _com TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./hal/.output

	cp -r ./hal/.output/obj/*.o ./.output/obj/
	cp -r ./$(ARCHNAME_MK)_Driver/.output/hal/obj/*.o ./.output/obj/
	cp -r ./$(ARCHNAME_MK)_Driver/.output/bsp/obj/*.o ./.output/obj/

	$(AR) rcs ./.output/lib/common.a ./.output/obj/*.o

_com :
	$(CC) $(CCFLAGS) $(CCINC) $(CCINC_COM) $(CCDEFS) -c ./*.c

	mv ./*.o ./obj/

clean :
	$(MAKE) clean TOP=$(TOP) -C ./$(ARCHNAME_MK)_Driver
	rm -rf ./.output
