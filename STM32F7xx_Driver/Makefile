PLATFORM ?= $(PLATFORM)
OUT ?= $(OUT)
TOP ?= $(TOP)
Q ?= @

include $(TOP)/configs/$(PLATFORM)/boot.mk

CCFLAGS := $(CCFLAGS_MK)
CCDEFS := $(CCDEFS_MK)
CCINC := $(CCINC_MK)
CCINC += -I$(TOP)/main/Inc \
		-I$(TOP)/common/Utilities/JPEG \
		-I$(TOP)/ulib/pub \
		-I$(TOP)/ulib/arch \
		-I$(TOP)/configs/$(PLATFORM) \
		$(HALINC_MK)

BSP_OBJ = .output/bsp/obj
HAL_OBJ = .output/hal/obj

bsp :
	@echo "Compiling $(BRDNAME_MK) $@ lib..."

	@mkdir -p ./.output/bsp/Components
	@mkdir -p ./.output/bsp/src
	@mkdir -p ./.output/bsp/obj

	@cp -r ./BSP/STM32F769I-Discovery/* ./.output/bsp/src
	@cp -r ./BSP/Components/Common ./.output/bsp/Components

ifeq ($(HAVE_HDMI), 1)
	@cp -r ./BSP/Components/adv7533 ./.output/bsp/Components
endif

ifeq ($(HAVE_LCD), 1)
	@cp -r ./BSP/Components/otm8009a ./.output/bsp/Components
endif

ifeq ($(HAVE_AUDIO), 1)
	@cp -r ./BSP/Components/wm8994 ./.output/bsp/Components
endif

ifeq ($(HAVE_TOUCH_SCREEN), 1)
	@cp -r ./BSP/Components/ft6x06 ./.output/bsp/Components
endif

	$(Q) $(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./.output/bsp/Components/*/*.c
	$(Q) $(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./.output/bsp/src/*.c

	@mv ./*.o ./.output/bsp/obj/
	@cp -r ./.output/bsp/obj/*.o $(OUT)

hal :
	@echo "Compiling $(BRDNAME_MK) $@ lib..."

	@mkdir -p ./.output/hal/src
	@mkdir -p ./.output/hal/obj

	@cp -r ./Src/* ./.output/hal/src

	$(Q) @$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) -c ./.output/hal/src/*.c

	@mv ./*.o ./.output/hal/obj
	@cp -r ./.output/hal/obj/*.o $(OUT)

hal/clean:
	@rm -rf ./.output/hal

bsp/clean:
	@rm -rf ./.output/bsp

clean :
	@rm -rf ./.output

