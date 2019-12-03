#ifndef __TIM_H__
#define __TIM_H__

typedef enum {
    TIM_NONE,
    TIM_RUNIT,
    TIM_RUNREG,
} timflags_t;

typedef struct timer_desc_s {
    void *parent;
    TIM_TypeDef *hw;
    TIM_HandleTypeDef handle;
    IRQn_Type irq;
    irqmask_t irqmask;
    void (*handler) (struct timer_desc_s *);
    void (*init) (struct timer_desc_s *);
    void (*deinit) (struct timer_desc_s *);
    uint32_t period;
    uint32_t presc;
    timflags_t flags;
} timer_desc_t;

int hal_tim_init (timer_desc_t *desc);
int hal_tim_deinit (timer_desc_t *desc);

#endif /*__TIM_H__*/
