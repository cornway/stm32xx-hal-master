#ifndef __BSP_CMD_INT_H__
#define __BSP_CMD_INT_H__

#include <bsp_api.h>
#include <bsp_cmd.h>

typedef struct cmdexec_s {
    struct cmdexec_s *next;
    char *text;
    uint32_t len;
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

int bsp_exec_link (arch_word_t *, const char *);
void cmd_parm_load_str (cmd_keyval_t *kv, const char *str);
void cmd_parm_load_val (cmd_keyval_t *kv, const char *str);
int cmd_parm_collect (cmd_keyval_t **, cmd_keyval_t **, int,
                                        const char **, const char **);

void __print_cmd_map (const cmd_func_map_t *map, int cnt);
#define PRINT_CMD_MAP(map) __print_cmd_map(map, arrlen(map))

#endif /*__BSP_CMD_INT_H__*/

