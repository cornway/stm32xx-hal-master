#if !defined(APPLICATION) || defined(BSP_DRIVER)

#include <tim_int.h>
#include <misc_utils.h>

extern uint32_t SystemCoreClock;

typedef struct tim_int_s {
    struct tim_int_s *next;
    timer_desc_t *desc;
} tim_int_t;

static tim_int_t *timer_desc_head = NULL;

#define TIM_MAX 6

tim_int_t tim_int_pool[TIM_MAX];
tim_int_t *last_free_tim;


static tim_int_t *tim_alloc (void)
{
    if (last_free_tim == NULL) {
        last_free_tim = &tim_int_pool[0];
    }
    if (last_free_tim == &tim_int_pool[TIM_MAX]) {
        return NULL;
    }
    return last_free_tim++;
}

static void tim_free (tim_int_t *tim)
{
    /*TODO :!!*/
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
    /*TODO :!!*/
}

int hal_tim_init (timer_desc_t *desc)
{
    TIM_HandleTypeDef *handle = &desc->handle;
    uint32_t prescaler = (uint32_t)((SystemCoreClock / 2) / desc->presc) - 1;
    irqmask_t irqmask;
    tim_int_t *tim;

    handle->Instance = desc->hw;

    handle->Init.Period            = desc->period - 1;
    handle->Init.Prescaler         = prescaler;
    handle->Init.ClockDivision     = 0;
    handle->Init.CounterMode       = TIM_COUNTERMODE_UP;
    handle->Init.RepetitionCounter = 0;
    handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    tim = tim_alloc();
    if (!tim) {
        return -1;
    }
    tim->desc = desc;
    tim_link(tim);
    
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

    }
    return 0;
}

int hal_tim_deinit (timer_desc_t *desc)
{
    TIM_HandleTypeDef *handle = &desc->handle;
    HAL_TIM_Base_Stop_IT(handle);
    HAL_TIM_Base_DeInit(handle);
    tim_unlink(container_of(desc, tim_int_t, desc));
    return 0;
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    tim_int_t *tim = timer_desc_head;
    
    assert(tim);

    while (tim) {
        if (&tim->desc->handle == htim && tim->desc->deinit) {
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
        if (&tim->desc->handle == htim && tim->desc->init) {
            tim->desc->init(tim->desc);
            return;
        }
        tim = tim->next;
    }
    assert(0);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    tim_int_t *tim = timer_desc_head;

    assert(tim);
    while (tim) {
        if (&tim->desc->handle == htim && tim->desc->handler) {
            tim->desc->handler(tim->desc);
            return;
        }
        tim = tim->next;
    }
    assert(0);
}

#endif

