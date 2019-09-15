#include <bsp_api.h>
#include <debug.h>
#include <bsp_sys.h>

char *test_api_info (void)
{
    dprintf("====================================\n");
    dprintf("TEST MODULE\n");
    dprintf("Build : %s %s\n", __DATE__, __TIME__);
    dprintf("====================================\n");

    return "TEST MODULE";
}

int main (int argc, const char **argv)
{
    dprintf("TEST MODULE+\n");
    while (argc) {
        dprintf("- %S -", argv[0]);
        argv++;
    }
    dprintf("TEST MODULE-\n");
    return 0;
}

void __arch_get_shared (void *sp, void *size)
{
    *(uint32_t *)sp = 0x20000000;
    *(uint32_t *)size = 0x00001000;
}

void system_ctor (void)
{
    g_bspapi = bsp_api_attach();
}
