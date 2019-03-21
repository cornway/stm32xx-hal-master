#ifndef __NVIC_H__
#define __NVIC_H__

#if (NVIC_IRQ_MAX > 32)
#error "uint32_t -> uint64_t"
#else
typedef uint32_t irqmask_t;
#endif

void irq_save (irqmask_t *flags);
void irq_save_mask(irqmask_t *flags, irqmask_t mask);
void irq_restore (irqmask_t flags);
void irq_bmap (irqmask_t *flags);
void NVIC_dump (void);

#endif /*__NVIC_H__*/
