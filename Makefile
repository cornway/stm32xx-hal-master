
f769_hal :
	$(MAKE) $(F769_HAL_TGT) TOP=$(TOP) -C ./STM32F7xx_Driver

clean :
	$(MAKE) clean TOP=$(TOP) -C ./STM32F7xx_Driver

