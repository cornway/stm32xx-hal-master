#ifndef _AUDIO_INT_H
#define _AUDIO_INT_H

#ifndef bool
typedef enum {false, true} bool;
#endif

#ifndef arrlen
#define arrlen(a) sizeof(a) / sizeof(a[0])
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member)     \
    (type *)((uint8_t *)(ptr) - offsetof(type,member))
#endif

#ifndef A_COMPILE_TIME_ASSERT
#define A_COMPILE_TIME_ASSERT(name, x)               \
       typedef int A_dummy_ ## name[(x) * 2 - 1]
#endif

#define AUDIO_MODULE_PRESENT 1
#define MUSIC_MODULE_PRESENT 0
#define USE_STEREO 0
#define USE_REVERB 0
#define COMPRESSION 1
#define USE_FLOAT 1


/*TODO use define instead of 0*/
#define AUDIO_PLAY_SCHEME 0

extern void music_tickle (void);
extern snd_sample_t *music_get_next_chunk (int32_t *size);
extern uint8_t music_get_volume (void);
extern int music_init (void);

#define AUDIO_TIMEOUT_MAX 2000 /*2 s*/
#define MAX_2BAND_VOL ((MAX_VOL) | (MAX_VOL << 8))
#define MAX_4BAND_VOL ((MAX_2BAND_VOL) | (MAX_2BAND_VOL << 16))

typedef struct {
    int size;
    snd_sample_t *buf;
    uint8_t volume;
} mixdata_t;

typedef struct {
    snd_sample_t *buf;
    int samples;
    bool *durty;
} a_buf_t;

typedef struct a_channel_head_s a_channel_head_t;
typedef struct a_channel_s a_channel_t;

struct a_channel_s {
    a_channel_t *next;
    a_channel_t *prev;

    audio_channel_t inst;
    int loopsize;
    snd_sample_t *bufposition;
    uint8_t volume;
#if USE_STEREO
    uint8_t left, right;
#endif
    uint8_t effect;
};

struct a_channel_head_s {
    a_channel_t       *first,
                      *last;
    uint16_t size;
    void (*empty_handle) (struct a_channel_head_s *head);
    void (*first_link_handle) (struct a_channel_head_s *head);
    void (*remove_handle) (struct a_channel_head_s *head, a_channel_t *rem);
};

typedef enum {
    A_ISR_NONE,
    A_ISR_HALF,
    A_ISR_COMP,
    A_ISR_MAX,
} isr_status_e;

#define chan_len(chan) \
    ((chan)->inst.chunk.alen)

#define chan_buf(chan) \
    ((chan)->inst.chunk.abuf)

#define chan_vol(chan) \
    ((chan)->inst.chunk.volume)

#define chan_is_play(chan) \
    ((chan)->inst.is_playing)

#define chan_cache(chan) \
    ((chan)->inst.chunk.cache)

#define chan_complete(chan) \
    (chan)->inst.complete

#define chan_loopstart(chan) \
    (chan)->inst.chunk.loopstart

#define chan_foreach_safe(head, channel, next) \
for (channel = (head)->first,\
     next = channel->next;   \
     channel;                \
     channel = next,         \
     next = next->next)


int a_channel_link (a_channel_head_t *head, a_channel_t *link, uint8_t sort);

int a_channel_unlink (a_channel_head_t *head, a_channel_t *node);

void a_channel_remove (a_channel_t *desc);

void a_paint_buffer (a_channel_head_t *chanlist, a_buf_t *abuf, int compratio);

uint8_t a_chanlist_try_reject_all (a_channel_head_t *chanlist);

#if USE_REVERB

void
a_rev_init (void);

#endif /*USE_REVERB*/

void
a_mem_init (void);

void
a_get_master_base (a_buf_t *master);

void
a_get_master4idx (a_buf_t *master, int idx);


void
a_grab_mixdata (a_channel_t *channel, a_buf_t *track, mixdata_t *mixdata);

void a_clear_abuf (a_buf_t *abuf);
void a_clear_master (void);

void error_handle (void);

#endif /*_AUDIO_INT_H*/
