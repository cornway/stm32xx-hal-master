

int d_open (char *path, int *hndl, char const * att);
void d_close (int h);
void d_seek (int handle, int position);
int d_eof (int handle);
int d_read (int handle, void *dst, int count);
char *d_gets (int handle, char *dst, int count);
int d_write (int handle, void *src, int count);
int d_mkdir (char *path);
uint32_t d_time (void);


