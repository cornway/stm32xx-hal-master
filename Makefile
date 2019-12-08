PLATFORM ?= $(PLATFORM)
TOP ?= $(TOP)
OUT ?= $(OUT)
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

CCINC_COM := -I$(TOP)/common/int

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
	@mkdir -p ./$(HAL_OBJ)
	$(MAKE) hal TOP=$(TOP) PLATFORM=$(PLATFORM) OUT=../$(HAL_OBJ) Q=$(Q) -C ./$(ARCHNAME_MK)_Driver
	@cp -r $(HAL_OBJ)/*.o $(OUT)

$(BSP_OBJ)/*.o :
	@mkdir -p ./$(BSP_OBJ)
	$(MAKE) bsp TOP=$(TOP) PLATFORM=$(PLATFORM) OUT=../$(BSP_OBJ) Q=$(Q) -C ./$(ARCHNAME_MK)_Driver
	@cp -r $(BSP_OBJ)/*.o $(OUT)

$(COM_OBJ)/*.o :
	@echo "Compiling $(BRDNAME_MK) hal..."

	@mkdir -p ./.output/obj/com
	@mkdir -p  ./hal/.output

	@cp -r ./hal/*.c ./hal/.output
ifeq ($(HAVE_JPEG), 1)
	@cp -r ./Utilities/JPEG/*.c ./hal/.output
endif

	$(Q) $(CC) $(CCFLAGS) $(CCINC) $(CCINC_COM) $(CCDEFS) -c ./hal/.output/*.c

	@mv ./*.o $(COM_OBJ)
	@cp -r $(COM_OBJ)/*.o $(OUT)

clean :
	$(MAKE) clean TOP=$(TOP) -C ./$(ARCHNAME_MK)_Driver
	@rm -rf ./hal/.output
	@rm -rf ./.output