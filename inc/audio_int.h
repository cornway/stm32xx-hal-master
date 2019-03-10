#ifndef _AUDIO_INT_H
#define _AUDIO_INT_H

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

extern snd_sample_t audio_raw_buf[2][AUDIO_OUT_BUFFER_SIZE];
#if COMPRESSION
extern uint8_t comp_weight;
#endif

#define AUDIO_TIMEOUT_MAX 1000 /*2 s*/
#define MAX_2BAND_VOL ((MAX_VOL) | (MAX_VOL << 8))
#define MAX_4BAND_VOL ((MAX_2BAND_VOL) | (MAX_2BAND_VOL << 16))

typedef struct a_channel_head_s a_channel_head_t;
typedef struct a_channel_s a_channel_t;

struct a_channel_s {
    a_channel_t *next;
    a_channel_t *prev;
    
    audio_channel_t inst;
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

#define chan_foreach(head, cur) \
for (a_channel_t *cur = (head)->first,\
    *__next = cur->next;               \
     cur;                              \
     cur = __next,                     \
     __next = __next->next)


int
a_channel_link (a_channel_head_t *head, a_channel_t *link, uint8_t sort);

int
a_channel_unlink (a_channel_head_t *head, a_channel_t *node);

#if USE_REVERB

void
a_rev_init (void);

int
a_rev_proc (snd_sample_t *dest);

void
a_rev_push (snd_sample_t s);

snd_sample_t
a_rev_pop (void);

void
a_rev2_push (snd_sample_t s);

snd_sample_t
a_rev2_pop (void);

#endif /*USE_REVERB*/

void
a_chanlist_move_window (a_channel_t *desc,
                           int size,
                           snd_sample_t **pbuf,
                           int *psize);

#if (AUDIO_PLAY_SCHEME == 1)

static inline uint8_t
a_chanlist_move_window_all ( int size,
                        uint16_t **pbuf,
                        int *psize);

#endif /*(AUDIO_PLAY_SCHEME == 1)*/

void
a_mix_single_to_master (snd_sample_t *dest,
                          snd_sample_t **ps,
                          uint8_t *vol,
                          int min_size);

void a_clear_master_idx (uint8_t idx, int force);
void a_clear_master_all (void);
void a_mark_master_idx (int idx);

void error_handle (void);

#endif /*_AUDIO_INT_H*/
