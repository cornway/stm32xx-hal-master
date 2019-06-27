#include <stdint.h>

typedef struct {
    /*Must be pointer to a static*/
    const char *cmd;
    const char *text;
    void *user1, *user2;
} cmdexec_t;

int str_parse_tok (const char *str, const char *tok, uint32_t *val);

typedef void (*cmd_handler_t) (const char *cmd, int cmdlen);

void exec_cmd_push (const char *cmd, const char *text, void *user1, void *user2);
cmdexec_t *exec_cmd_pop (cmdexec_t *cmd);
void exec_iterate_cmd (cmd_handler_t hdlr);
void exec_cmd_execute (const char *cmd, int len);



