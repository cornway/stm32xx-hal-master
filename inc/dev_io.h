
typedef enum {
    FTYPE_FILE,
    FTYPE_DIR,
} ftype_t;

typedef int (*list_clbk_t)(char *name, ftype_t type);

typedef struct {
    list_clbk_t clbk;
    void *user;
} flist_t;

int dev_io_init (void);
int d_open (char *path, int *hndl, char const * att);
int d_size (int hndl);
void d_close (int h);
void d_unlink (char *path);
void d_seek (int handle, int position);
int d_eof (int handle);
int d_read (int handle, void *dst, int count);
char *d_gets (int handle, char *dst, int count);
int d_write (int handle, void *src, int count);
int d_mkdir (char *path);
uint32_t d_time (void);
int d_dirlist (char *path, flist_t *flist);


