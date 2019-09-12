#ifndef __MPU_H__
#define __MPU_H__

#ifdef CPU_CHACHELINE
#define MPU_CACHELINE CPU_CHACHELINE
#else
#define MPU_CACHELINE 32
#endif

void mpu_init (void);
void mpu_deinit (void);
int mpu_lock (arch_word_t addr, arch_word_t *size, const char *mode);
int mpu_unlock (arch_word_t addr, arch_word_t size);
int mpu_read (arch_word_t addr, arch_word_t size);



#endif /*__MPU_H__*/

