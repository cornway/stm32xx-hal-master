#include <stdint.h>
#include <stddef.h>
#include "audio_main.h"
#include "audio_int.h"

snd_sample_t audio_raw_buf[2][AUDIO_OUT_BUFFER_SIZE];

static uint8_t buf_cleared[2] = {0, 0};


int
a_channel_link (a_channel_head_t *head, a_channel_t *link, uint8_t sort)
{
    head->size++;
    if (head->first == NULL) {
        head->first = link;
        head->last = link;
        link->next = NULL;
        link->prev = NULL;
        if (head->first_link_handle)
            head->first_link_handle(head);
        return head->size;
    }
    chan_foreach(head, cur) {
        if ((chan_len(link) < chan_len(cur))) {
            if (cur->prev != NULL) {
                cur->prev->next = link;
                link->next = cur;
                link->prev = cur->prev;
                cur->prev = link;
                return head->size;
            }
            link->next = cur;
            link->prev = NULL;
            cur->prev = link;
            head->first = link;
            return head->size;
         }
    }
    link->prev = head->last;
    link->next = NULL;
    head->last->next = link;
    head->last = link;
    return head->size;                 
}

int
a_channel_unlink (a_channel_head_t *head, a_channel_t *node)
{
    if (!head->size) {
        return 0;
    }
    head->size--;
    a_channel_t *prev = node->prev, *next = node->next;
    if (!head->size) {
        head->first = NULL;
        head->last = NULL;

        if (head->remove_handle)
            head->remove_handle(head, node);

        if (head->empty_handle)
            head->empty_handle(head);
        return 0;
    }
    if (!prev) {
        head->first = next;
        next->prev = NULL;
    } else if (next) {
        prev->next = next;
        next->prev = prev;
    } else if (!next) {
        prev->next = NULL;                    
        head->last = prev;
    }
    if (head->remove_handle)
            head->remove_handle(head, node);

    return head->size;
}


#if USE_REVERB

#define REVERB_END_BUFFER (0x800)
#define REVERB_END_BUFFER_M (REVERB_END_BUFFER - 1)

#define REVERB2_END_BUFFER (0x100)
#define REVERB2_END_BUFFER_M (REVERB2_END_BUFFER - 1)

static snd_sample_t reverb_raw_buf[REVERB_END_BUFFER];
static snd_sample_t reverb2_raw_buf[REVERB2_END_BUFFER];


static uint16_t  rev_rd_idx = 0, rev_wr_idx = 0;
static uint16_t  rev2_rd_idx = 0, rev2_wr_idx = 0;

void a_rev_init (void)
{
    int i;
    for (i = 0; i < REVERB_END_BUFFER; i++) {
        a_rev_push(0);
    }
    for (i = 0; i < REVERB2_END_BUFFER; i++) {
        a_rev2_push(0);
    }
}

int a_rev_proc (snd_sample_t *dest)
{
    int i = 0;
    snd_sample_t s;
    int mark_to_clear = 0;

    for (i = 0; i < AUDIO_OUT_BUFFER_SIZE; i++) {
        s = a_rev_pop();
        if (s) {
            dest[i] = dest[i] / 2 + s / 2;
            mark_to_clear++;
        }
        a_rev_push(dest[i]);
        s = a_rev2_pop();
        if (s) {
            dest[i] = dest[i] / 2 + s / 2;
            mark_to_clear++;
        }
        a_rev2_push(dest[i]);
    }
    return mark_to_clear;
}

void
a_rev_push (snd_sample_t s)
{
    reverb_raw_buf[(rev_rd_idx++) & REVERB_END_BUFFER_M] = s;
}

snd_sample_t
a_rev_pop (void)
{
    return reverb_raw_buf[(rev_wr_idx++) & REVERB_END_BUFFER_M];
}

void
a_rev2_push (snd_sample_t s)
{
    reverb2_raw_buf[(rev2_rd_idx++) & REVERB2_END_BUFFER_M] = s;
}

snd_sample_t
a_rev2_pop (void)
{
    return reverb2_raw_buf[(rev2_wr_idx++) & REVERB2_END_BUFFER_M];
}

#endif /*USE_REVERB*/

void
a_chanlist_move_window (a_channel_t *desc,
                           int size,
                           snd_sample_t **pbuf,
                           int *psize)
{
    Mix_Chunk *chunk = &desc->inst.chunk;

    if (chan_is_play(desc)== 0) {
        return;
    }

    *pbuf = chunk->abuf;
    *psize = size;

    if (chunk->alen < size)
        *psize = chunk->alen;

    chunk->abuf += *psize;
    chunk->alen -= *psize;
}

#if (AUDIO_PLAY_SCHEME == 1)

static inline uint8_t
a_chanlist_move_window_all ( int size,
                        uint16_t **pbuf,
                        int *psize)
{
    int i = 0;
    chan_foreach(&chan_llist_ready, cur) {
        a_chanlist_move_window(cur, size, &pbuf[i], &psize[i]);
        i++;
    }
    return chan_llist_ready.size;
}

#endif /*(AUDIO_PLAY_SCHEME == 1)*/

void a_clear_master_idx (uint8_t idx, int force)
{
    uint32_t *p_buf = (uint32_t *)audio_raw_buf[idx];
    if (buf_cleared[idx] && !force)
        return;

    for (int i = 0; i < AUDIO_OUT_BUFFER_SIZE / 2; i++) {
        p_buf[i] = 0;
    }
    buf_cleared[idx] = 1;
}

void a_clear_master_all (void)
{
    uint32_t *p_buf = (uint32_t *)audio_raw_buf[0];

    for (int i = 0; i < AUDIO_OUT_BUFFER_SIZE; i++) {
        p_buf[i] = 0;
    }
    buf_cleared[0] = 1;
    buf_cleared[1] = 1;
}

void a_mark_master_idx (int idx)
{
    buf_cleared[0] = 0;
}

#define AMP(x, vol) (((int16_t)x * vol) / MAX_VOL)
#define COMP(x, vol, comp) (((int16_t)x * vol) / comp)

#define AMP_FLT(x, vol) (int16_t)((float)(x) * (float)vol)

void
a_mix_single_to_master (snd_sample_t *dest,
                          snd_sample_t **ps,
                          uint8_t *vol,
                          int min_size)
{
    int16_t *pdest = (int16_t *)dest;
    int16_t *psrc = (int16_t *)ps[0];
    uint8_t _vol = vol[0];
#if USE_FLOAT
    float vol_flt;
#endif
#if COMPRESSION
    int16_t weight;
#endif

    if (_vol == 0)
        return;

#if (COMPRESSION == 0)

    if (_vol == MAX_VOL) {
        for (int i = 0; i < min_size; i++) {
            pdest[i] = psrc[i] / 2 + pdest[i] / 2;
        }
    } else {
#if USE_FLOAT
        vol_flt = (float)(_vol) / (float)(MAX_VOL);
        for (int i = 0; i < min_size; i++) {
            pdest[i] = AMP_FLT(psrc[i], vol_flt) + pdest[i];
        }
#else
        for (int i = 0; i < min_size; i++) {
            pdest[i] = AMP(psrc[i], _vol) + pdest[i];
        }
#endif
    }
#else

    if (_vol == MAX_VOL) {
        for (int i = 0; i < min_size; i++) {
            pdest[i] = psrc[i] / comp_weight  + pdest[i];
        }
    } else {
        weight = comp_weight * MAX_VOL;
#if USE_FLOAT
        vol_flt = (float)(_vol) / (float)(weight);
        for (int i = 0; i < min_size; i++) {
            pdest[i] = AMP_FLT(psrc[i], vol_flt) + pdest[i];
        }
#else
        for (int i = 0; i < min_size; i++) {
            pdest[i] = COMP(psrc[i], _vol, weight) + pdest[i];
        }
#endif
    }
#endif
}


#if (AUDIO_PLAY_SCHEME == 1)

static inline void
mix_to_master_raw2 (snd_sample_t *dest,
                          snd_sample_t **ps,
                          uint8_t *vol,
                          int min_size)
{
    int16_t *pdest = (int16_t *)dest;
    int16_t *psrc0 = (int16_t *)ps[0];
    int16_t *psrc1 = (int16_t *)ps[1];
    uint16_t band_vol = *((uint32_t *)vol);
    int16_t t = 0;

#if (COMPRESSION == 0)
    
    if (band_vol == MAX_2BAND_VOL) {
        for (int i = 0; i < min_size; i++) {
            t = psrc0[i] / 2 + psrc1[i] / 2;
            pdest[i] = t + pdest[i] / 2;
        }
    } else {
        a_mix_single_to_master(dest, ps, vol, min_size);
        a_mix_single_to_master(dest, ps + 1, vol + 1, min_size);
    }
#else
    if (band_vol == MAX_2BAND_VOL) {
        for (int i = 0; i < min_size; i++) {
            t = psrc0[i] / comp_weight + psrc1[i] / comp_weight;
            pdest[i] = t + pdest[i];
        }
    } else {
        a_mix_single_to_master(dest, ps, vol, min_size);
        a_mix_single_to_master(dest, ps + 1, vol + 1, min_size);
    }
#endif

}

static inline void
mix_to_master_raw4 (snd_sample_t *dest,
                          snd_sample_t **ps,
                          uint8_t *vol,
                          int min_size)
{
    int16_t *pdest = (int16_t *)dest;
    int16_t *psrc0 = (int16_t *)ps[0];
    int16_t *psrc1 = (int16_t *)ps[1];
    int16_t *psrc2 = (int16_t *)ps[2];
    int16_t *psrc3 = (int16_t *)ps[3];
    uint32_t band_vol = *((uint32_t *)vol);
    int16_t t = 0;
#if (COMPRESSION == 0)

    if (band_vol == MAX_4BAND_VOL) {
        for (int i = 0; i < min_size; i++) {
            t = psrc0[i] / 2 + psrc1[i] / 2;
            t += psrc2[i] / 2 + psrc3[i] / 2;
            pdest[i] = t + pdest[i] / 2;
        }
    } else {
        mix_to_master_raw2(dest, ps, vol, min_size);
        mix_to_master_raw2(dest, ps + 2, vol + 2, min_size);
    }
#else

    if (band_vol == MAX_4BAND_VOL) {
        for (int i = 0; i < min_size; i++) {
            t = psrc0[i] / comp_weight + psrc1[i] / comp_weight;
            t += psrc2[i] / comp_weight + psrc3[i] / comp_weight;
            pdest[i] = t + pdest[i] / comp_weight;
        }
    } else {
        mix_to_master_raw2(dest, ps, vol, min_size);
        mix_to_master_raw2(dest, ps + 2, vol + 2, min_size);
    }

#endif
}

static void chunk_proc_raw_all (snd_sample_t *dest,
                                     snd_sample_t **ps,
                                     uint8_t *vol,
                                     int cnt,
                                     int data_size)
{
    switch (cnt) {

        case 1:
            a_mix_single_to_master(dest, ps, vol, data_size);
        break;

        case 2:
            mix_to_master_raw2(dest, ps, vol, data_size);
        break;

        case 3:
            mix_to_master_raw2(dest, ps, vol, data_size);
            a_mix_single_to_master(dest, ps + 2, vol + 2, data_size);
        break;

        case 4:
            mix_to_master_raw4(dest, ps, vol, data_size);
        break;

        case 5:
            mix_to_master_raw4(dest, ps, vol, data_size);
            a_mix_single_to_master(dest, ps + 4, vol + 4, data_size);
        break;

        case 6:
            mix_to_master_raw4(dest, ps, vol, data_size);
            mix_to_master_raw2(dest, ps + 4, vol + 4, data_size);
        break;

        case 7:
            mix_to_master_raw4(dest, ps, vol, data_size);
            mix_to_master_raw2(dest, ps + 4, vol + 4, data_size);
            a_mix_single_to_master(dest, ps + 6, vol + 6, data_size);
        break;

        case 8:
            mix_to_master_raw4(dest, ps, vol, data_size);
            mix_to_master_raw4(dest, ps + 4, vol + 4, data_size);
        break;

        default: error_handle();
    }
}

static uint8_t
chan_to_vol_arr (uint8_t *vol)
{
    int16_t i = 0;
    Mix_Chunk *chunk;
    chan_foreach(&chan_llist_ready, cur) {
        chunk = &cur->inst.chunk;
        vol[i] = chunk->volume;
        i++;
    }
    return chan_llist_ready.size;
}

static uint8_t
chan_to_buf_arr (uint16_t **ps)
{
    int16_t i = 0;
    Mix_Chunk *chunk;
    chan_foreach(&chan_llist_ready, cur) {
        chunk = &cur->inst.chunk;
        ps[i] = chunk->abuf;
        i++;
    }
    return chan_llist_ready.size;
} 

#endif /*(AUDIO_PLAY_SCHEME == 1)*/
