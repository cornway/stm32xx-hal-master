#ifndef __BSP_CMD_INT_H__
#define __BSP_CMD_INT_H__

#include <bsp_api.h>

typedef int (*cmd_func_t) (int argc, char **argv);
typedef void (*cmd_handler_t) (const char *cmd, int cmdlen);

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

#endif /*__BSP_CMD_INT_H__*/

