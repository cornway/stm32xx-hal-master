#include "main.h"
#include "nvic.h"
#include "dev_conf.h"

#ifndef USE_STM32F769I_DISCO
#error "Not supported"
#endif

#define InvalidIrqNum (NonMaskableInt_IRQn - 1)

FlagStatus initialized = RESET;
static IRQn_Type irq_maptable[NVIC_IRQ_MAX];
static int irq_maptable_index = 0;

irqmask_t irq_active_mask = 0, irq_saved_mask = 0;

static void NVIC_init_table (void)
{
    int i;

    for (i = 0; i < NVIC_IRQ_MAX; i++) {
        irq_maptable[i] = InvalidIrqNum;
    }

    initialized = SET;
}

static void NVIC_map_irq (IRQn_Type IRQn)
{
    int i;
    if (irq_maptable_index >= NVIC_IRQ_MAX) {
        fatal_error("irq_maptable_index >= NVIC_IRQ_MAX");
    }
    for (i = 0; i < irq_maptable_index; i++) {
        if (irq_maptable[i] == IRQn) {
            return;
        }
    }
    irq_active_mask = irq_active_mask | (1 << irq_maptable_index);
    irq_maptable[irq_maptable_index] = IRQn;
    irq_maptable_index++;
}

static inline void _irq_save (irqmask_t *flags, irqmask_t mask)
{
    int i;

    mask = irq_active_mask & (~mask);
    mask = (~irq_saved_mask) & mask;

    *flags = mask;

    for (i = 0; i < irq_maptable_index;) {
        if (((mask >> i) & 0xff) == 0) {
            i += 8;
            continue;
        }
        if ((mask >> i) & 0x1) {
            NVIC_DisableIRQ(irq_maptable[i]);
        }
        i++;
    }
    *flags = mask;
    irq_saved_mask |= mask;
}

void irq_save (irqmask_t *flags)
{
    _irq_save(flags, 0);
}

void irq_save_mask(irqmask_t *flags, irqmask_t mask)
{
    _irq_save(flags, mask);
}

void irq_restore (irqmask_t flags)
{
    int i;
    for (i = 0; i < irq_maptable_index;) {
        if (((flags >> i) & 0xff) == 0) {
            i += 8;
            continue;
        }
        if ((flags >> i) & 0x1) {
            NVIC_EnableIRQ(irq_maptable[i]);
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

    NVIC_map_irq(IRQn);

    prioritygroup = NVIC_GetPriorityGrouping();

    NVIC_SetPriority(IRQn, NVIC_EncodePriority(prioritygroup, PreemptPriority, SubPriority));
}



