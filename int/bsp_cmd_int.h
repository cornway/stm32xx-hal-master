#ifndef __BSP_CMD_INT_H__
#define __BSP_CMD_INT_H__

#include <bsp_api.h>

#define CMD_MAX_NAME        (16)
#define CMD_MAX_PATH        (128)
#define CMD_MAX_BUF         (256)
#define CMD_MAX_ARG         (16)
#define CMD_MAX_RECURSION    (16)

typedef int (*cmd_func_t) (int, const char **);
typedef int (*cmd_handler_t) (const char *, int);

typedef enum {
    CMDERR_OK,
    CMDERR_GENERIC,
    CMDERR_NOARGS,
    CMDERR_NOPATH,
    CMDERR_INVPARM,
    CMDERR_PERMISS,
    CMDERR_UNKNOWN,
    CMDERR_MAX,
} cmd_errno_t;

typedef enum {
    DVAR_FUNC,
    DVAR_INT32,
    DVAR_FLOAT,
    DVAR_STR,
} dvar_obj_t;

typedef struct {
    void *ptr;
    uint16_t ptrsize;
    uint16_t size;
    dvar_obj_t type;
    uint32_t flags;
} cmdvar_t;

typedef struct {
    char *text;
    int len;
    void *user1, *user2;
} cmdexec_t;

typedef struct {
    const char *name;
    cmd_func_t func;
} cmd_func_map_t;

typedef struct cmd_keyval_s {
    const char *key;
    void *val;
    const char fmt[3];
    uint8_t valid;
    void (*handle) (struct cmd_keyval_s *, const char *);
} cmd_keyval_t;

/**_S - means 'short', for '-x' cases*/
#define CMD_KVI32_S(c, v) \
    {c, v, "%i", 0, NULL}

#define CMD_KVX32_S(c, v) \
    {c, v, "%x", 0, NULL}

#define CMD_KVSTR_S(c, v) \
    {c, v, "%s", 0, NULL}

cmd_func_t bsp_stdin_unstash (cmd_func_t);
cmd_func_t bsp_stdin_stash (cmd_func_t);
int bsp_exec_link (arch_word_t *, const char *);
int boot_cmd_handle (int , const char **);

void cmd_parm_load_str (cmd_keyval_t *kv, const char *str);
void cmd_parm_load_val (cmd_keyval_t *kv, const char *str);
int cmd_parm_collect (cmd_keyval_t **, cmd_keyval_t **, int,
                                        const char **, const char **);

int cmd_init (void);
void cmd_deinit (void);
int cmd_unregister (const char *);
int cmd_txt_exec (const char *, int);
void cmd_exec_dsr (const char *, const char *,
                        void *, void *);

void __print_cmd_map (const cmd_func_map_t *map, int cnt);
#define PRINT_CMD_MAP(map) __print_cmd_map(map, arrlen(map))

#endif /*__BSP_CMD_INT_H__*/

