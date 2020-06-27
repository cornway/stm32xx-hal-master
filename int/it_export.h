#ifndef __IT_EXPORT_H__
#define __IT_EXPORT_H__

#ifdef __cplusplus
    extern "C" {
#endif

extern void NMI_Handler(void);
extern void HardFault_Handler(void);
extern void MemManage_Handler(void);
extern void BusFault_Handler(void);
extern void UsageFault_Handler(void);
extern void SVC_Handler(void);
extern void DebugMon_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);
extern void AUDIO_IN_SAI_PDMx_DMAx_IRQHandler(void);
extern void AUDIO_OUT_SAIx_DMAx_IRQHandler(void);

extern void BSP_SDMMC_DMA_Tx_IRQHandler (void);
extern void BSP_SDMMC_DMA_Rx_IRQHandler (void);
extern void SDMMC2_IRQHandler (void);

extern void USART1_IRQHandler (void);

extern void TIM1_IRQHandler (void);
extern void TIM2_IRQHandler (void);
extern void TIM3_IRQHandler (void);
extern void TIM4_IRQHandler (void);
extern void TIM5_IRQHandler (void);
extern void TIM6_IRQHandler (void);


extern void DMA1_Stream1_IRQHandler (void);
extern void DMA1_Stream2_IRQHandler (void);
extern void DMA1_Stream3_IRQHandler (void);
extern void DMA1_Stream4_IRQHandler (void);
extern void DMA1_Stream5_IRQHandler (void);
extern void DMA1_Stream6_IRQHandler (void);
extern void DMA1_Stream7_IRQHandler (void);
extern void DMA2_Stream1_IRQHandler (void);
extern void DMA2_Stream2_IRQHandler (void);
extern void DMA2_Stream3_IRQHandler (void);
extern void DMA2_Stream4_IRQHandler (void);
extern void DMA2_Stream5_IRQHandler (void);
extern void DMA2_Stream6_IRQHandler (void);
extern void DMA2_Stream7_IRQHandler (void);


#ifdef __cplusplus
    }
#endif

#endif /* __IT_EXPORT_H__ */
