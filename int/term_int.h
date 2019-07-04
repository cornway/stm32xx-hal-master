#include <stdint.h>
#include "bsp_cmd_int.h"

int str_parse_tok (const char *str, const char *tok, uint32_t *val);
int str_tokenize (const char **tok, int tokcnt, char *str);
void str_filter_printable (char *str);

void bsp_stdin_register_if (cmd_func_t clbk);
void bsp_stdout_register_if (cmd_func_t clbk);
void bsp_stdin_unreg_if (cmd_func_t clbk);
void bsp_stout_unreg_if (cmd_func_t clbk);

void hexdump_u8 (const uint8_t *data, int len, int rowlength);
void hexdump_le_u32 (const uint32_t *data, int len, int rowlength);


