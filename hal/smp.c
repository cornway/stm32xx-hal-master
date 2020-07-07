
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <stm32h7xx.h>

#include <arch.h>
#include <bsp_api.h>
#include <smp.h>
#include <misc_utils.h>
#include <heap.h>

extern bspapi_t *_bsp_api_attach (arch_word_t *ptr, arch_word_t size);
extern size_t _bsp_api_size (void);

void *m_pool_init (void *pool, size_t size);
void m_init (void);
void *m_malloc (void *pool, size_t size, const char *caller_name);
void *m_malloc_align (void *pool, uint32_t size, uint32_t align);
void m_free (void *p);
size_t m_avail (void *pool);
void m_stat (void);

#define MAX_HSEM_ID 32

extern const char *__cm4_shared_base;
extern const char *__cm4_shared_limit;

typedef struct {
    int id;
    char name[16];
} hsem_t;

typedef struct {
    hsem_t pool[MAX_HSEM_ID];
} hsem_pool_t;

typedef struct {
    hal_smp_task_t *head;
    hal_smp_task_t *tail;
} task_list_t;

hsem_pool_t *hsem_pool;
void *task_pool;
task_list_t *task_list;
int core_id = 0;

int hal_smp_init (int _core_id)
{
    uint8_t *mem = (uint8_t *)&__cm4_shared_base;
    uint8_t *mem_end = (uint8_t *)&__cm4_shared_limit;
    hsem_t *hsem;
    int id;

    hsem_pool = (hsem_pool_t *)mem;
    mem += sizeof(hsem_pool_t);
    task_list = (task_list_t *)mem;

    g_bspapi = _bsp_api_attach((arch_word_t *)(mem_end - _bsp_api_size()), _bsp_api_size());

    if (_core_id) {
        return 0;
    }
    core_id = _core_id;
    d_memzero(hsem_pool, sizeof(hsem_pool_t));
    for (id = 0; id < arrlen(hsem_pool->pool); id++) {
        hsem = &hsem_pool->pool[id];
        hsem->id = -1;
    }

    mem += sizeof(task_list_t);
    task_pool = m_pool_init(mem, mem_end - mem - _bsp_api_size());
    task_list->head = NULL;
    task_list->tail = NULL;
    
    __HAL_RCC_HSEM_CLK_ENABLE();
    return 0;
}

int hal_smp_deinit (void)
{
    return 0;
}

int hal_smp_hsem_alloc (const char *name)
{
    hsem_t *hsem;
    int namelen = strlen(name) + 1;
    int id, first_avail = -1;

    if (namelen >= arrlen(hsem_pool->pool[0].name)) {
        return -1;
    }
    for (id = 0; id < arrlen(hsem_pool->pool); id++) {
        hsem = &hsem_pool->pool[id];
        if (hsem->id >= 0) {
            if (!strcmp(hsem->name, name)) {
                return hsem->id;
            }
        } else if (first_avail < 0) {
            first_avail = id;
        }
    }
    if (first_avail >= 0) {
        hsem = &hsem_pool->pool[first_avail];
        snprintf(hsem->name, sizeof(hsem->name), "%s", name);
        hsem->id = first_avail;

        if (core_id) {
            HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(hsem->id));
        }
    }
    return first_avail;
    
}

int hal_smp_hsem_destroy (int s)
{
    hsem_t *hsem = &hsem_pool->pool[s];
    hsem->id = -1;
    return 0;
}

int hal_smp_hsem_lock (int s)
{
    return !HAL_HSEM_FastTake(s);
}

int hal_smp_hsem_spinlock (int s)
{
    while (HAL_HSEM_FastTake(s)) {}
    return 1;
}

int hal_smp_hsem_release (int s)
{
    HAL_HSEM_Release(s, core_id);
    return 0;
}

hal_smp_task_t *hal_smp_sched_task (void (*func) (void *), void *usr, size_t usr_size)
{
    hal_smp_task_t *task;

    task = (hal_smp_task_t *)m_malloc(task_pool, sizeof(hal_smp_task_t) + usr_size, __func__);

    if (!task) {
        return NULL;
    }

    task->func = func;
    task->next = NULL;
    task->usr_size = usr_size;
    if (usr) {
        task->arg = task + 1;
        d_memcpy(task->arg, usr, usr_size);
    } else {
        task->arg = NULL;
    }
    if (!task_list->head) {
        task_list->head = task;
    } else {
        task_list->tail->next = task;
    }
    task_list->tail = task;

    task->pending = 1;
    return task;
}

hal_smp_task_t *hal_smp_next_task (void)
{
    hal_smp_task_t *task = task_list->head;
    if (task) {
        task->pending = 0;
        task->executing = 1;
        if (task_list->tail == task) {
            task_list->head = NULL;
            task_list->tail = NULL;
        } else {
            task_list->head = task->next;
        }
    }
    return task;
}

void hal_smp_remove_task (hal_smp_task_t *task)
{
    task->executing = 0;
    m_free(task);
}


