#include "main.h"
#include "nvic.h"
#include "dev_conf.h"
#include "debug.h"
#include "misc_utils.h"

#ifndef USE_STM32F769I_DISCO
#error "Not supported"
#endif

#define InvalidIrqNum ((IRQn_Type)(NonMaskableInt_IRQn - 1))

typedef struct {
    IRQn_Type irq;
    uint8_t preempt, preemptsub;
    uint8_t group;
} irq_desc_t;

#if NVIC_IRQ_MAX <= 32
#define NVIC_IRQ_MASK (0xffffffffU)
#endif


FlagStatus initialized = RESET;
static irq_desc_t irq_maptable[NVIC_IRQ_MAX];
static int irq_maptable_index = 0;

irqmask_t irq_active_mask = 0, irq_saved_mask = 0;

static void NVIC_init_table (void)
{
    int i;

    for (i = 0; i < NVIC_IRQ_MAX; i++) {
        irq_maptable[i].irq = InvalidIrqNum;
    }

    initialized = SET;
}

/*
    Sys tick timer is used everywere by HAL to handle timeouts\delays,
    But i have to disable ALL non-fatal interrupts to let SD card op's be atomic.
    SDMMC may report a Rx overrun error if interrupted during FIFO reading.
    FIXME : avoid disabling sys tick irq
*/
static inline void NVIC_SysTickIrqCtrl (d_bool disable)
{
    uint32_t ctrl = SysTick->CTRL;
    if (disable) {
        ctrl &= ~SysTick_CTRL_TICKINT_Msk;
    } else {
        ctrl |= SysTick_CTRL_TICKINT_Msk;
    }
    SysTick->CTRL = ctrl;
}

static inline void NVIC_SysIrqCtrl (IRQn_Type irq, d_bool disable)
{
    switch (irq) {
        case SysTick_IRQn:
            NVIC_SysTickIrqCtrl(disable);
        break;
        default:
            assert(0);
    }
}

static void NVIC_map_irq (IRQn_Type IRQn, uint8_t preempt, uint8_t preemptsub, uint8_t group)
{
    int i;
    if (irq_maptable_index >= NVIC_IRQ_MAX) {
        fatal_error("irq_maptable_index >= NVIC_IRQ_MAX");
    }
    for (i = 0; i < irq_maptable_index; i++) {
        if (irq_maptable[i].irq == IRQn) {
            return;
        }
    }
    irq_active_mask = irq_active_mask | (1 << irq_maptable_index);
    irq_maptable[irq_maptable_index].irq = IRQn;
    irq_maptable[irq_maptable_index].preempt = preempt;
    irq_maptable[irq_maptable_index].group = group;
    irq_maptable[irq_maptable_index].preemptsub = preemptsub;
    irq_maptable_index++;
}

static inline void _irq_save (irqmask_t *flags)
{
    int i;
    irqmask_t mask = *flags;
    IRQn_Type irq;

    if (!mask)
        mask = NVIC_IRQ_MASK;
    else
        assert((irq_active_mask & mask) == mask);

    mask = irq_active_mask & mask;
    mask = ~irq_saved_mask & mask;

    *flags = mask;

    for (i = 0; i < irq_maptable_index;) {
        if (((mask >> i) & 0xff) == 0) {
            i += 8;
            continue;
        }
        if ((mask >> i) & 0x1) {
            irq = irq_maptable[i].irq;
            if (irq < 0) {
                NVIC_SysIrqCtrl(irq, d_true);
            } else {
                NVIC_DisableIRQ(irq_maptable[i].irq);
            }
        }
        i++;
    }
    *flags = mask;
    irq_saved_mask |= mask;
}

void irq_save (irqmask_t *flags)
{
    _irq_save(flags);
}

void irq_restore (irqmask_t flags)
{
    int i;
    IRQn_Type irq;

    for (i = 0; i < irq_maptable_index;) {
        if (((flags >> i) & 0xff) == 0) {
            i += 8;
            continue;
        }
        if ((flags >> i) & 0x1) {
            irq = irq_maptable[i].irq;
            if (irq < 0) {
                NVIC_SysIrqCtrl(irq, d_false);
            } else {
                NVIC_EnableIRQ(irq);
            }
        }
        i++;
    }
    irq_saved_mask &= ~flags;
}

void irq_bmap (irqmask_t *flags)
{
    *flags = irq_active_mask;
}

void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority)
{ 
    uint32_t prioritygroup = 0x00;

    if (initialized == RESET) {
        NVIC_init_table();
    }
    /* Check the parameters */
    assert_param(IS_NVIC_SUB_PRIORITY(SubPriority));
    assert_param(IS_NVIC_PREEMPTION_PRIORITY(PreemptPriority));

    prioritygroup = NVIC_GetPriorityGrouping();

    NVIC_map_irq(IRQn, PreemptPriority, SubPriority, prioritygroup);

    NVIC_SetPriority(IRQn, NVIC_EncodePriority(prioritygroup, PreemptPriority, SubPriority));
}



const char *NVIC_CortexM7_name[] =
{
/******  Cortex-M7 Processor Exceptions Numbers ****************************************************************/
  "NonMaskableInt_IRQn",//         = -14,    /*!< 2 Non Maskable Interrupt                                          */
  "MemoryManagement_IRQn",//       = -12,    /*!< 4 Cortex-M7 Memory Management Interrupt                           */
  "BusFault_IRQn",//               = -11,    /*!< 5 Cortex-M7 Bus Fault Interrupt                                   */
  "UsageFault_IRQn",//             = -10,    /*!< 6 Cortex-M7 Usage Fault Interrupt                                 */
  "SVCall_IRQn",//                 = -5,     /*!< 11 Cortex-M7 SV Call Interrupt                                    */
  "DebugMonitor_IRQn",//           = -4,     /*!< 12 Cortex-M7 Debug Monitor Interrupt                              */
  "PendSV_IRQn",//                 = -2,     /*!< 14 Cortex-M7 Pend SV Interrupt                                    */
  "SysTick_IRQn",//                = -1,     /*!< 15 Cortex-M7 System Tick Interrupt                                */
};

static const IRQn_Type NVIC_Cortex_map[] =
{
    (IRQn_Type)-14, (IRQn_Type)-12, (IRQn_Type)-11, (IRQn_Type)-10,
    (IRQn_Type)-5, (IRQn_Type)-4, (IRQn_Type)-2, (IRQn_Type)-1,
};

A_COMPILE_TIME_ASSERT(NvicCortexNames, arrlen(NVIC_CortexM7_name) == arrlen(NVIC_Cortex_map));


const char *NVIC_STM32_name[] =
{
/******  STM32 specific Interrupt Numbers **********************************************************************/
  "WWDG_IRQn",//                   = 0,      /*!< Window WatchDog Interrupt                                         */
  "PVD_IRQn",//                    = 1,      /*!< PVD through EXTI Line detection Interrupt                         */
  "TAMP_STAMP_IRQn",//             = 2,      /*!< Tamper and TimeStamp interrupts through the EXTI line             */
  "RTC_WKUP_IRQn",//               = 3,      /*!< RTC Wakeup interrupt through the EXTI line                        */
  "FLASH_IRQn",//                  = 4,      /*!< FLASH global Interrupt                                            */
  "RCC_IRQn",//                    = 5,      /*!< RCC global Interrupt                                              */
  "EXTI0_IRQn",//                  = 6,      /*!< EXTI Line0 Interrupt                                              */
  "EXTI1_IRQn",//                  = 7,      /*!< EXTI Line1 Interrupt                                              */
  "EXTI2_IRQn",//                  = 8,      /*!< EXTI Line2 Interrupt                                              */
  "EXTI3_IRQn",//                  = 9,      /*!< EXTI Line3 Interrupt                                              */
  "EXTI4_IRQn",//                  = 10,     /*!< EXTI Line4 Interrupt                                              */
  "DMA1_Stream0_IRQn",//           = 11,     /*!< DMA1 Stream 0 global Interrupt                                    */
  "DMA1_Stream1_IRQn",//           = 12,     /*!< DMA1 Stream 1 global Interrupt                                    */
  "DMA1_Stream2_IRQn",//           = 13,     /*!< DMA1 Stream 2 global Interrupt                                    */
  "DMA1_Stream3_IRQn",//           = 14,     /*!< DMA1 Stream 3 global Interrupt                                    */
  "DMA1_Stream4_IRQn",//           = 15,     /*!< DMA1 Stream 4 global Interrupt                                    */
  "DMA1_Stream5_IRQn",//           = 16,     /*!< DMA1 Stream 5 global Interrupt                                    */
  "DMA1_Stream6_IRQn",//           = 17,     /*!< DMA1 Stream 6 global Interrupt                                    */
  "ADC_IRQn",//                    = 18,     /*!< ADC1, ADC2 and ADC3 global Interrupts                             */
  "CAN1_TX_IRQn",//                = 19,     /*!< CAN1 TX Interrupt                                                 */
  "CAN1_RX0_IRQn",//               = 20,     /*!< CAN1 RX0 Interrupt                                                */
  "CAN1_RX1_IRQn",//               = 21,     /*!< CAN1 RX1 Interrupt                                                */
  "CAN1_SCE_IRQn",//               = 22,     /*!< CAN1 SCE Interrupt                                                */
  "EXTI9_5_IRQn",//                = 23,     /*!< External Line[9:5] Interrupts                                     */
  "TIM1_BRK_TIM9_IRQn",//          = 24,     /*!< TIM1 Break interrupt and TIM9 global interrupt                    */
  "TIM1_UP_TIM10_IRQn",//          = 25,     /*!< TIM1 Update Interrupt and TIM10 global interrupt                  */
  "TIM1_TRG_COM_TIM11_IRQn",//     = 26,     /*!< TIM1 Trigger and Commutation Interrupt and TIM11 global interrupt */
  "TIM1_CC_IRQn",//                = 27,     /*!< TIM1 Capture Compare Interrupt                                    */
  "TIM2_IRQn",//                   = 28,     /*!< TIM2 global Interrupt                                             */
  "TIM3_IRQn",//                   = 29,     /*!< TIM3 global Interrupt                                             */
  "TIM4_IRQn",//                   = 30,     /*!< TIM4 global Interrupt                                             */
  "I2C1_EV_IRQn",//                = 31,     /*!< I2C1 Event Interrupt                                              */
  "I2C1_ER_IRQn",//                = 32,     /*!< I2C1 Error Interrupt                                              */
  "I2C2_EV_IRQn",//                = 33,     /*!< I2C2 Event Interrupt                                              */
  "I2C2_ER_IRQn",//                = 34,     /*!< I2C2 Error Interrupt                                              */
  "SPI1_IRQn",//                   = 35,     /*!< SPI1 global Interrupt                                             */
  "SPI2_IRQn",//                   = 36,     /*!< SPI2 global Interrupt                                             */
  "USART1_IRQn",//                 = 37,     /*!< USART1 global Interrupt                                           */
  "USART2_IRQn",//                 = 38,     /*!< USART2 global Interrupt                                           */
  "USART3_IRQn",//                 = 39,     /*!< USART3 global Interrupt                                           */
  "EXTI15_10_IRQn",//              = 40,     /*!< External Line[15:10] Interrupts                                   */
  "RTC_Alarm_IRQn",//              = 41,     /*!< RTC Alarm (A and B) through EXTI Line Interrupt                   */
  "OTG_FS_WKUP_IRQn",//            = 42,     /*!< USB OTG FS Wakeup through EXTI line interrupt                     */
  "TIM8_BRK_TIM12_IRQn",//         = 43,     /*!< TIM8 Break Interrupt and TIM12 global interrupt                   */
  "TIM8_UP_TIM13_IRQn",//          = 44,     /*!< TIM8 Update Interrupt and TIM13 global interrupt                  */
  "TIM8_TRG_COM_TIM14_IRQn",//     = 45,     /*!< TIM8 Trigger and Commutation Interrupt and TIM14 global interrupt */
  "TIM8_CC_IRQn",//                = 46,     /*!< TIM8 Capture Compare Interrupt                                    */
  "DMA1_Stream7_IRQn",//           = 47,     /*!< DMA1 Stream7 Interrupt                                            */
  "FMC_IRQn",//                    = 48,     /*!< FMC global Interrupt                                              */
  "SDMMC1_IRQn",//                 = 49,     /*!< SDMMC1 global Interrupt                                           */
  "TIM5_IRQn",//                   = 50,     /*!< TIM5 global Interrupt                                             */
  "SPI3_IRQn",//                   = 51,     /*!< SPI3 global Interrupt                                             */
  "UART4_IRQn",//                  = 52,     /*!< UART4 global Interrupt                                            */
  "UART5_IRQn",//                  = 53,     /*!< UART5 global Interrupt                                            */
  "TIM6_DAC_IRQn",//               = 54,     /*!< TIM6 global and DAC1&2 underrun error  interrupts                 */
  "TIM7_IRQn",//                   = 55,     /*!< TIM7 global interrupt                                             */
  "DMA2_Stream0_IRQn",//           = 56,     /*!< DMA2 Stream 0 global Interrupt                                    */
  "DMA2_Stream1_IRQn",//           = 57,     /*!< DMA2 Stream 1 global Interrupt                                    */
  "DMA2_Stream2_IRQn",//           = 58,     /*!< DMA2 Stream 2 global Interrupt                                    */
  "DMA2_Stream3_IRQn",//           = 59,     /*!< DMA2 Stream 3 global Interrupt                                    */
  "DMA2_Stream4_IRQn",//           = 60,     /*!< DMA2 Stream 4 global Interrupt                                    */
  "ETH_IRQn",//                    = 61,     /*!< Ethernet global Interrupt                                         */
  "ETH_WKUP_IRQn",//               = 62,     /*!< Ethernet Wakeup through EXTI line Interrupt                       */
  "CAN2_TX_IRQn",//                = 63,     /*!< CAN2 TX Interrupt                                                 */
  "CAN2_RX0_IRQn",//               = 64,     /*!< CAN2 RX0 Interrupt                                                */
  "CAN2_RX1_IRQn",//               = 65,     /*!< CAN2 RX1 Interrupt                                                */
  "CAN2_SCE_IRQn",//               = 66,     /*!< CAN2 SCE Interrupt                                                */
  "OTG_FS_IRQn",//                 = 67,     /*!< USB OTG FS global Interrupt                                       */
  "DMA2_Stream5_IRQn",//           = 68,     /*!< DMA2 Stream 5 global interrupt                                    */
  "DMA2_Stream6_IRQn",//           = 69,     /*!< DMA2 Stream 6 global interrupt                                    */
  "DMA2_Stream7_IRQn",//           = 70,     /*!< DMA2 Stream 7 global interrupt                                    */
  "USART6_IRQn",//                 = 71,     /*!< USART6 global interrupt                                           */
  "I2C3_EV_IRQn",//                = 72,     /*!< I2C3 event interrupt                                              */
  "I2C3_ER_IRQn",//                = 73,     /*!< I2C3 error interrupt                                              */
  "OTG_HS_EP1_OUT_IRQn",//         = 74,     /*!< USB OTG HS End Point 1 Out global interrupt                       */
  "OTG_HS_EP1_IN_IRQn",//          = 75,     /*!< USB OTG HS End Point 1 In global interrupt                        */
  "OTG_HS_WKUP_IRQn",//            = 76,     /*!< USB OTG HS Wakeup through EXTI interrupt                          */
  "OTG_HS_IRQn",//                 = 77,     /*!< USB OTG HS global interrupt                                       */
  "DCMI_IRQn",//                   = 78,     /*!< DCMI global interrupt                                             */
  "CRYP_IRQn",//                   = 79,     /*!< CRYP crypto global interrupt                                      */
  "HASH_RNG_IRQn",//               = 80,     /*!< Hash and Rng global interrupt                                     */
  "FPU_IRQn",//                    = 81,     /*!< FPU global interrupt                                              */
  "UART7_IRQn",//                  = 82,     /*!< UART7 global interrupt                                            */
  "UART8_IRQn",//                  = 83,     /*!< UART8 global interrupt                                            */
  "SPI4_IRQn",//                   = 84,     /*!< SPI4 global Interrupt                                             */
  "SPI5_IRQn",//                   = 85,     /*!< SPI5 global Interrupt                                             */
  "SPI6_IRQn",//                   = 86,     /*!< SPI6 global Interrupt                                             */
  "SAI1_IRQn",//                   = 87,     /*!< SAI1 global Interrupt                                             */
  "LTDC_IRQn",//                   = 88,     /*!< LTDC global Interrupt                                             */
  "LTDC_ER_IRQn",//                = 89,     /*!< LTDC Error global Interrupt                                       */
  "DMA2D_IRQn",//                  = 90,     /*!< DMA2D global Interrupt                                            */
  "SAI2_IRQn",//                   = 91,     /*!< SAI2 global Interrupt                                             */
  "QUADSPI_IRQn",//                = 92,     /*!< Quad SPI global interrupt                                         */
  "LPTIM1_IRQn",//                 = 93,     /*!< LP TIM1 interrupt                                                 */
  "CEC_IRQn",//                    = 94,     /*!< HDMI-CEC global Interrupt                                         */
  "I2C4_EV_IRQn",//                = 95,     /*!< I2C4 Event Interrupt                                              */
  "I2C4_ER_IRQn",//                = 96,     /*!< I2C4 Error Interrupt                                              */
  "SPDIF_RX_IRQn",//               = 97,     /*!< SPDIF-RX global Interrupt                                         */
  "DSI_IRQn"//                     = 98,     /*!< DSI global Interrupt*/
};

//NVIC_Cortex_map

char *CortexIrqName (IRQn_Type irq)
{
    int i;

    for (i = 0; i < arrlen(NVIC_CortexM7_name); i++) {
        if (NVIC_Cortex_map[i] == irq) {
            return (char *)NVIC_CortexM7_name[i];
        }
    }
    return NULL;
}

void NVIC_dump (void)
{
    int i;
    irq_desc_t *irq_desc;

    dprintf("%s : DUMP BEGIN\n", __func__);
    for (i = 0; i < irq_maptable_index; i++) {

        irq_desc = &irq_maptable[i];

        if (irq_desc->irq == InvalidIrqNum) {
            dprintf("slot[%d] : InvalidIrqNum !\n", i);
            continue;
        }
        if (irq_desc->irq < 0) {
            char * const name = CortexIrqName(irq_desc->irq);
            dprintf("slot[%d] : %d : %s (Cortex-M7 Processor Exception)\n",
                    i, irq_desc->irq,
                    name ? name : "Unknown");
            continue;
        }
        if (irq_desc->irq >= arrlen(NVIC_STM32_name)) {
            dprintf("slot[%d] : %d : Too large IRQ num\n", i, irq_desc->irq);
            continue;
        }
        dprintf("slot[%d] : %s, num : %d, prio : %d, subprio : %d, group : %d\n",
                i,
                NVIC_STM32_name[irq_desc->irq],
                irq_desc->irq,
                irq_desc->preempt,
                irq_desc->preemptsub,
                irq_desc->group);
    }
    dprintf("%s : DUMP END\n", __func__);
}

