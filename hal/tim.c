#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <main.h>
#include <arch.h>
#include <bsp_api.h>
#include <nvic.h>
#include <tim.h>
#include <misc_utils.h>
#include <debug.h>

typedef struct {
    TIM_TypeDef *hw;
    TIM_HandleTypeDef handle;
} tim_hal_t;

typedef struct tim_int_s {
    struct tim_int_s *next;
    tim_hal_t hal;
    timer_desc_t *desc;
    uint8_t alloced;
} tim_int_t;

static tim_int_t *timer_desc_head = NULL;

#define TIM_MAX 6

tim_int_t tim_int_pool[TIM_MAX] = {{0}};

static tim_int_t *tim_alloc (void)
{
    int i;
    for (i = 0; i < TIM_MAX; i++) {
        if (tim_int_pool[i].alloced == 0) {
            tim_int_pool[i].alloced = 1;
            return &tim_int_pool[i];
        }
    }
    return NULL;
}

static void tim_free (tim_int_t *tim)
{
    tim->alloced = 0;
}

static void tim_link (tim_int_t *tim)
{
    tim_int_t *head;

    tim->next = NULL;
    if (!timer_desc_head) {
        timer_desc_head = tim;
        return;
    }
    head = timer_desc_head;
    timer_desc_head = tim;
    tim->next = head;
}

static void tim_unlink (tim_int_t *tim)
{
    tim_int_t *prev = NULL, *cur = timer_desc_head;
    while (cur) {
        if (cur == tim) {
            if (prev) {
                prev->next = tim->next;
            } else {
                timer_desc_head = tim->next;
            }
            tim_free(tim);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    assert(0);
}

timer_desc_t *hal_timer_get_by_hw (void *hw)
{
    tim_int_t *tim = timer_desc_head;
    while (tim) {
        if (hw == (void *)tim->hal.hw) {
            return tim->desc;
        }
        tim = tim->next;
    }
    return NULL;
}


int hal_timer_init (timer_desc_t *desc, void *hw, irqn_t irqn)
{
    tim_int_t *tim;
    TIM_HandleTypeDef *handle;


    tim = tim_alloc();
    if (!tim) {
        return -1;
    }
    tim_link(tim);
    tim->desc = desc;
    desc->parent = tim;

    tim->hal.hw = (TIM_TypeDef *)hw;
    desc->irq = irqn;

    handle = &tim->hal.handle;
    uint32_t prescaler = (uint32_t)((SystemCoreClock / 2) / desc->presc) - 1;
    irqmask_t irqmask;
  
    handle->Instance = tim->hal.hw;

    handle->Init.Period            = desc->period - 1;
    handle->Init.Prescaler         = prescaler;
    handle->Init.ClockDivision     = 0;
    handle->Init.CounterMode       = TIM_COUNTERMODE_UP;
    handle->Init.RepetitionCounter = 0;
    handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    irq_bmap(&irqmask);
    if (HAL_TIM_Base_Init(handle) != HAL_OK)
    {
        return -1;
    }
    irq_bmap(&desc->irqmask);
    desc->irqmask = desc->irqmask & (~irqmask);

    if (desc->flags == TIM_RUNIT) {
        if (HAL_TIM_Base_Start_IT(handle) != HAL_OK)
        {
            return -1;
        }
    } else if (desc->flags == TIM_RUNREG) {
        if (HAL_TIM_Base_Start(handle) != HAL_OK)
        {
            return -1;
        }
    } else {
        assert(0);
    }
    return 0;
}

int hal_timer_deinit (timer_desc_t *desc)
{
    tim_int_t *tim = (tim_int_t *)desc->parent;
    TIM_HandleTypeDef *handle = &tim->hal.handle;

    if (desc->flags == TIM_RUNIT) {
        HAL_TIM_Base_Stop_IT(handle);
    } else if (desc->flags == TIM_RUNREG) {
        HAL_TIM_Base_Stop(handle);
    } else {
        assert(0);
    }
    HAL_TIM_Base_DeInit(handle);
    tim_unlink(tim);
    return 0;
}

static void hal_hires_timer_msp_init (timer_desc_t *desc);
static void hal_hires_timer_msp_deinit (timer_desc_t *desc);

int hal_hires_timer_init (timer_desc_t *desc)
{
    desc->init = hal_hires_timer_msp_init;
    desc->deinit = hal_hires_timer_msp_deinit;
    return hal_timer_init(desc, TIM2, TIM2_IRQn);
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    tim_int_t *tim = timer_desc_head;
    
    assert(tim);

    while (tim) {
        if (&tim->hal.handle == htim && tim->desc->deinit) {
            tim->desc->deinit(tim->desc);
            return;
        }
        tim = tim->next;
    }
    assert(0);
}

/*TODO : move this outside*/
extern TIM_HandleTypeDef profile_timer_handle;

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    tim_int_t *tim = timer_desc_head;

    assert(tim);

    while (tim) {
        if (&tim->hal.handle == htim) {
            if (tim->desc->init) {
                tim->desc->init(tim->desc);
            }
            return;
        }
        tim = tim->next;
    }
    dprintf("%s() : Unknown handler: <%p>\n", __func__, htim);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    tim_int_t *tim = timer_desc_head;

    assert(tim);
    while (tim) {
        if (&tim->hal.handle == htim && tim->desc->handler) {
            tim->desc->handler(tim->desc);
            return;
        }
        tim = tim->next;
    }
    assert(0);
}

void hal_timer_irq_handler (void *hw)
{
    timer_desc_t *desc = hal_timer_get_by_hw(hw);
    tim_int_t *tim = (tim_int_t *)desc->parent;

    HAL_TIM_IRQHandler(&tim->hal.handle);
}

uint32_t hal_timer_value (timer_desc_t *desc)
{
    tim_int_t *tim = (tim_int_t *)desc->parent;
    return tim->hal.hw->CNT;
}

static void hal_hires_timer_msp_init (timer_desc_t *desc)
{
    tim_int_t *tim = (tim_int_t *)desc->parent;

    if (tim->hal.hw == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}

static void hal_hires_timer_msp_deinit (timer_desc_t *desc)
{
    tim_int_t *tim = (tim_int_t *)desc->parent;

    if (tim->hal.hw == TIM2) {
        __HAL_RCC_TIM2_CLK_DISABLE();
    }
}

void TIM2_IRQHandler (void)
{
    hal_timer_irq_handler(TIM2);
}

void TIM3_IRQHandler (void)
{
    hal_timer_irq_handler(TIM3);
}


