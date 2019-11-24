
HALSRC := $(TOP)/common/STM32F7xx_Driver

CCINC := -I$(HALSRC)/Inc \
	-I$(HALSRC)/CMSIS/Device/ST/STM32F7xx/Include \
	-I$(HALSRC)/CMSIS/Include \
	-I$(TOP)/Inc \
	-I$(TOP)/ulib/pub \
	-I$(TOP)/ulib/arch

f769_hal :
	mkdir -p ./.output
	mkdir -p ./.output/obj
	mkdir -p ./.output/lib

	cp -r ./Src/* ./.output/
	cp Makefile ./.output/

	$(MAKE) _f769_hal TOP=$(TOP) -C ./.output

CCFLAGS := $$CCFLAGS_TOP
CCDEFS := $$CCDEFS_TOP
CC := $$CCBIN_TOP

CCFLAGS_F769 :=  -mcpu=cortex-m7 -fsingle-precision-constant -Wdouble-promotion \
		-mthumb -mthumb-interwork

CCDEFS_F769 := -DSTM32F769xx -D__ARCH_ARM_M7__
CCINC_F769 := -I$(TOP)/ulib/arch/Arm/m7/inc

_f769_hal :
	$(CC) $(CCFLAGS) $(CCINC) $(CCDEFS) $(CCINC_F769) $(CCFLAGS_F769) $(CCDEFS_F769) -c ./*.c

	mv ./*.o ./obj/
	ar rcs ./lib/hal.a ./obj/*.o

clean :
	rm -rf ./.output

