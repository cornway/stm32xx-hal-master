#ifndef __JPEG_H__
#define __JPEG_H__

#include "../utilities/jpeg/jpeg_utils.h"

int jpeg_init (const char *conf);
void *jpeg_cache (const char *path, uint32_t *size);
void jpeg_release (void *p);
int jpeg_decode (jpeg_info_t *info, void *tempbuf, void *data, uint32_t size);

#endif /*__JPEG_H__*/

