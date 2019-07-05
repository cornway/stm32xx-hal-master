#include <stdint.h>
#include "bsp_cmd_int.h"

int str_parse_tok (const char *str, const char *tok, uint32_t *val);
int str_tokenize (const char **tok, int tokcnt, char *str);
void str_collect_ascii (char *str);
void str_replace_2_ascii (char *str);
int str_insert_args (char *dest, const char *src, int argc, const char **argv);
int str_check_args_present (const char *str);

void bsp_stdin_register_if (cmd_func_t clbk);
void bsp_stdout_register_if (cmd_func_t clbk);
void bsp_stdin_unreg_if (cmd_func_t clbk);
void bsp_stout_unreg_if (cmd_func_t clbk);
int bsp_inout_forward (char *buf, int size, char dir);

void hexdump_u8 (const void *data, int len, int rowlength);
void hexdump_le_u32 (const void *data, int len, int rowlength);

typedef int (*printfmt_t) (int, const char *, ...);

int __hexdump_u8 (printfmt_t printfmt, int stream, const uint8_t *data, int len, int rowlength);
int __hexdump_le_u32 (printfmt_t printfmt, int stream, const uint32_t *data, int len, int rowlength);
void __hexdump (printfmt_t printfmt, int stream,
                  const void *data, int bits, int len, int rowlength);
void hexdump (const void *data, int bits, int len, int rowlength);


