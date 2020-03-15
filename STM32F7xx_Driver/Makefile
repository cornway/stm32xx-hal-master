PLATFORM ?= $(PLATFORM)
OUT ?= $(OUT)
TOP ?= $(TOP)
Q ?= @

include $(TOP)/boot.mk

BSP_OBJ = .output/bsp/obj
HAL_OBJ = .output/hal/obj

bsp :
	@echo "[ $(TGT_PLATFORM) $@... ]"

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

	$(Q) $(CC) $(CFLAGS) $$CINC $$INC_HAL $(CDEFS) -c ./.output/bsp/Components/*/*.c
	$(Q) $(CC) $(CFLAGS) $$CINC $$INC_HAL $(CDEFS) -c ./.output/bsp/src/*.c

	@mv ./*.o ./.output/bsp/obj/
	@cp -r ./.output/bsp/obj/*.o $(OUT)

hal :
	@echo "[ $(TGT_PLATFORM) $@... ]"

	@mkdir -p ./.output/hal/src
	@mkdir -p ./.output/hal/obj

	@cp -r ./Src/* ./.output/hal/src

	$(Q) $(CC) $(CFLAGS) $$CINC $$INC_HAL $(CDEFS) -c ./.output/hal/src/*.c

	@mv ./*.o ./.output/hal/obj
	@cp -r ./.output/hal/obj/*.o $(OUT)

hal/clean:
	@rm -rf ./.output/hal

bsp/clean:
	@rm -rf ./.output/bsp

clean :
	@rm -rf ./.output

