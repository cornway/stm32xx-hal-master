PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)
OUT ?= $(OUT)
Q ?= @

include $(TOP)/boot.mk

TGT_HAL_SRC:=$(TOP)/common/$(TGT_HAL_DIR)_Driver

INC_HAL:=-I$(TGT_HAL_SRC)/Inc
INC_HAL+=-I$(TGT_HAL_SRC)/CMSIS/Device/ST/$(TGT_HAL_DIR)/Include
INC_HAL+=-I$(TGT_HAL_SRC)/CMSIS/Include
INC_HAL+=-I$(TGT_HAL_SRC)/BSP/Components
INC_HAL+=-I$(TGT_HAL_SRC)/BSP/STM32F769I-Discovery
INC_HAL+=-I$(TOP)/common/Utilities/JPEG
INC_HAL+=-I$(TOP)/configs/$(PLATFORM)
export INC_HAL

CCINC := $$CINC $$INC_HAL -I$(TOP)/common/int

OBJ := .output/obj
OUT_OBJ := .output/obj
HAL_OBJ := $(OBJ)/hal
BSP_OBJ := $(OBJ)/bsp
COM_OBJ := $(OBJ)/com

.PHONY: hal bsp com
hal : hal/hal hal/bsp hal/com

hal/hal: $(HAL_OBJ)/*.o
hal/bsp: $(BSP_OBJ)/*.o
hal/com : $(COM_OBJ)/*.o

$(HAL_OBJ)/*.o :
	@echo "[ $(TGT_PLATFORM) $@... ]"
	@mkdir -p ./$(HAL_OBJ)
	@$(MAKE) hal TOP=$(TOP) PLATFORM=$(PLATFORM) OUT=../$(HAL_OBJ) Q=$(Q) -C ./$(TGT_HAL_DIR)_Driver
	@cp -r $(HAL_OBJ)/*.o $(OUT)

$(BSP_OBJ)/*.o :
	@echo "[ $(TGT_PLATFORM) $@... ]"
	@mkdir -p ./$(BSP_OBJ)
	@$(MAKE) bsp TOP=$(TOP) PLATFORM=$(PLATFORM) OUT=../$(BSP_OBJ) Q=$(Q) -C ./$(TGT_HAL_DIR)_Driver
	@cp -r $(BSP_OBJ)/*.o $(OUT)

$(COM_OBJ)/*.o :
	@echo "[ $(TGT_PLATFORM) $@... ]"

	@mkdir -p ./.output/obj/com
	@mkdir -p  ./hal/.output

	@cp -r ./hal/*.c ./hal/.output
ifeq ($(HAVE_JPEG), 1)
	@cp -r ./Utilities/JPEG/*.c ./hal/.output
endif

	$(Q) $(CC) $(CFLAGS) $(CCINC) $(CDEFS) -c ./hal/.output/*.c

	@mv ./*.o $(COM_OBJ)
	@cp -r $(COM_OBJ)/*.o $(OUT)

clean :
	@$(MAKE) clean TOP=$(TOP) -C ./$(TGT_HAL_DIR)_Driver
	@rm -rf ./hal/.output
	@rm -rf ./.output