#include "stdint.h"
#include "string.h"
#include "debug.h"
#include "main.h"

#if 0


#define P_RECORDS_MAX 1024
#define P_MAX_DEEPTH 36

#if DEBUG_SERIAL

#define prints(args ...) dprintf(args)

#else

#define prints(args ...)

#endif

enum {
    PFLAG_ENTER = (1 << 0),
    PFLAG_EXIT  = (1 << 1),
};

typedef struct {
    char const      *func;
    uint32_t        cycles;
    uint8_t         levelsdeep;
    uint8_t         flags;
    uint16_t        next;
} record_t;

typedef struct {
    int16_t top, bottom;
} rhead_t;

static record_t records_pool[P_RECORDS_MAX];
static uint16_t last_alloced_record = 0;

static rhead_t record_levels[P_MAX_DEEPTH];
static int16_t profile_deepth = 0;

extern uint32_t SystemCoreClock;

static const float us_in_seconds = 1.0f / 1000000.0f;

static uint32_t cpu_cycles_count;
static float cpu_time_per_cycle;

static void delay_us (uint32_t us)
{
    volatile uint32_t cycles = (uint32_t)((float)us * us_in_seconds * (float)SystemCoreClock);

    while (cycles--)
    {
    }
}

static inline record_t *prof_alloc_rec (void)
{
    if (last_alloced_record >= P_RECORDS_MAX) {
        return NULL;
    }
    return &records_pool[last_alloced_record++];
}

static void prof_link_rec (record_t *rec)
{
    rhead_t *head = &record_levels[rec->levelsdeep];
    int idx = (int)(rec - records_pool);
    record_t *prev_rec;

    rec->next = -1;

    if (head->top < 0) {
        head->top = idx;
        head->bottom = idx;
        return;
    }
    prev_rec = records_pool[head->top];
    prev_rec->next = idx;
    head->top = idx;
}

void profiler_enter (const char *func, int line)
{
    record_t *rec;

    if (profile_deepth >= P_MAX_DEEPTH) {
        return;
    }

    rec = prof_alloc_rec();

    if (!rec) {
        return;
    }
    rec->func = func;
    rec->levelsdeep = profile_deepth++;
    rec->flags |= PFLAG_ENTER;
    rec->cycles = DWT->CYCCNT;

    prof_link_rec(rec);
}

void profiler_exit (const char *func, int line)
{
    record_t *rec;

    if (profile_deepth <= 0) {
        fatal_error("");
    }

    rec = prof_alloc_rec();

    if (!rec) {
        return;
    }
    rec->func = func;
    rec->levelsdeep = --profile_deepth;
    rec->flags |= PFLAG_EXIT;
    rec->cycles = DWT->CYCCNT;

    prof_link_rec(rec);
}

void profiler_reset (void)
{
    int i;
    memset(records_pool, 0, sizeof(records_pool));

    for (i = 0; i < P_MAX_DEEPTH; i++) {
        record_levels[i].top = -1;
        record_levels[i].bottom = -1;
    }

    last_alloced_record = 0;
    profile_deepth = 0;
}

void profiler_init (void)
{
    cpu_time_per_cycle = 1.0f / ((float)SystemCoreClock);
    DWT->CTRL |= 1 ; // enable the counter
    DWT->CYCCNT = 0; // reset the counter
    delay_us(1);
    cpu_cycles_count = DWT->CYCCNT;
    cpu_cycles_count--;

    profiler_reset();
}

void profiler_print (void)
{
    char charbuf[256];
    rhead_t *head = NULL, *prev_head = NULL;
    int i;

    for (i = 0; i < P_MAX_DEEPTH; i++) {

        head = &record_levels[i];
        if (head->top == 0) {
            break;
        }


        prev_head = head;
    }
}

#endif
