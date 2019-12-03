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

.PHONY: hal
hal :
	mkdir -p ./.output/obj

	$(MAKE) hal TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./$(ARCHNAME_MK)_Driver
	cp -r ./$(ARCHNAME_MK)_Driver/.output/hal/obj/*.o ./.output/obj/

.PHONY: bsp
bsp :
	mkdir -p ./.output/obj

	$(MAKE) bsp TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./$(ARCHNAME_MK)_Driver
	cp -r ./$(ARCHNAME_MK)_Driver/.output/bsp/obj/*.o ./.output/obj/


CCINC_COM := -I$(TOP)/common/int

.PHONY: com
com :
	mkdir -p ./.output/obj

	mkdir -p  ./hal/.output/obj
	mkdir -p  ./hal/.output/lib

	cp -r ./hal/*.c ./hal/.output
ifeq ($(HAVE_JPEG), 1)
	cp -r ./Utilities/JPEG/*.c ./hal/.output
endif
	cp ./Makefile ./hal/.output

	$(MAKE) _com TOP=$(TOP) PLATFORM=$(PLATFORM) -C ./hal/.output

	cp -r ./hal/.output/obj/*.o ./.output/obj/

	#$(AR) rcs ./.output/lib/common.a ./.output/obj/*.o

_com :
	$(CC) $(CCFLAGS) $(CCINC) $(CCINC_COM) $(CCDEFS) -c ./*.c

	mv ./*.o ./obj/

clean :
	$(MAKE) clean TOP=$(TOP) -C ./$(ARCHNAME_MK)_Driver

	rm -rf ./hal/.output
	rm -rf ./.output
