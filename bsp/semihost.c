
#include <debug.h>
#include <bsp_sys.h>

void _sys_exit(int return_code)
{
    dprintf("Exit code : %d\n", return_code);
    
    for (;;) {}
}

void _ttywrch (int ch)
{
    serial_putc(ch);
}

char *_sys_command_string(char *cmd, int len)
{
    return bsp_sys_cmd_get(cmd, len);
}

#define SYSCALL_INSMOD_ID      (0x1U)
#define SYSCALL_INSMOD_DONE_ID (0x2U)
#define SYSCALL_EXIT           (0x3U)

typedef struct {
    CPU_STACK_FRAME cpu_ctxt;

    unsigned status:   3;
    unsigned fpu_ctxt: 1;
    unsigned flags:    4;

    uint8_t cpu_hw_priv;
} cpu_ctxt_desc_t;

static cpu_ctxt_desc_t *cpu_ctxt_desc_active = NULL;

static void cpu_save_ctxt_ptr (cpu_ctxt_desc_t *t, arch_sysio_arg_t arg)
{
    if ((arg.LINK & EXC_RETURN_USE_FPU_BM) == 0) {
        t->fpu_ctxt = 1;
    } else {
        t->fpu_ctxt = 1;
    }
    t->cpu_ctxt = arg.FRAME;
}

static arch_word_t cpu_update_lr (cpu_ctxt_desc_t *t)
{
    if (t->fpu_ctxt) {
        if ((t->cpu_hw_priv & CPU_USE_PSP) == CPU_USE_PSP) {
            return EXC_RETURN(THREAD_FPU_PSP);
        } else {
            return EXC_RETURN(THREAD_FPU_MSP);
        }
    } else if ((t->cpu_hw_priv & CPU_USE_PSP) == CPU_USE_PSP) {
            return EXC_RETURN(THREAD_NOFPU_PSP);
    }
    return EXC_RETURN(THREAD_NOFPU_MSP);
}

static inline void
__sys_get_user_io_args
        (arch_sysio_arg_t *user, CPU_STACK_FRAME *cpu_frame, arch_word_t flags)
{
    arch_cpu_ctxt_set_reg(cpu_frame, flags, POINTER, user->r0);
    arch_cpu_ctxt_set_reg(cpu_frame, flags, OPTION_A,user->r1);
    arch_cpu_ctxt_set_reg(cpu_frame, flags, OPTION_B,user->r2);
    arch_cpu_ctxt_set_reg(cpu_frame, flags, ERROR,   user->r3);
}

extern ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t
    __arch_sysio_call (ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t);

arch_sysio_arg_t __syscall_ret_fuse (int error)
{
    arch_sysio_arg_t arg = {SYSCALL_EXIT, error, 0, 0};
    return __arch_sysio_call(arg);
}

static arch_sysio_arg_t
__syscall_insmod (arch_sysio_arg_t *user)
{
    arch_sysio_arg_t ret;
    cpu_ctxt_desc_t *user_desc = (cpu_ctxt_desc_t *)user->r2;
    CPU_STACK_FRAME *cpu_frame = user_desc->cpu_ctxt;

    cpu_ctxt_desc_active = user_desc;
    cpu_frame->callControl.PC = user->r2;
    cpu_frame->callControl.PSR = CPU_XPSR_T_BM;
    cpu_frame->cpuStack.LR = (arch_word_t)__syscall_ret_fuse;

    arch_cpu_ctxt_set_reg(cpu_frame, ret.lr, ERROR, 0);

    ret.lr     = cpu_update_lr(user_desc);
    ret.ptr    = user_desc->cpu_ctxt;
    ret.ctrl   = user_desc->cpu_hw_priv;
}

static arch_sysio_arg_t
__syscall_insmod_done (arch_sysio_arg_t *user)
{
    
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t
__arch_svc_handler (ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t args)
{
    arch_sysio_arg_t user_args, ret_args;
    cpu_ctxt_desc_t cpu_ctxt_desc;
    CPU_STACK_FRAME *cpu_frame;

    cpu_save_ctxt_ptr(&cpu_ctxt_desc, args);
    cpu_frame = &cpu_ctxt_desc.cpu_ctxt;

    __sys_get_user_io_args(&user_args, cpu_frame, args.lr);

    if (user_args.r0 == 0) {
        d_memcpy(&ret_args, &user_args, sizeof(ret_args));
        goto svc_done;
    }

    switch (user_args.r0 & 0xff) {
        case SYSCALL_INSMOD_ID:
            ret_args = __syscall_insmod(&user_args);
        break;
        case SYSCALL_INSMOD_DONE_ID:
            ret_args = __syscall_insmod_done(&user_args);
        break;
        default:
            d_memcpy(&ret_args, &user_args, sizeof(ret_args));
            goto svc_done;
        break;
    };

svc_done:
    return ret_args;
}

int sys_mod_insert (void *entry)
{
    cpu_ctxt_desc_t cpu_ctxt_desc = {0};
    arch_sysio_arg_t io_regs;

    cpu_ctxt_desc.cpu_hw_priv = CPU_ACCESS_LEVEL_2;

    io_regs.r0 = SYSCALL_INSMOD_ID;
    io_regs.r1 = entry;
    io_regs.r2 = (arch_word_t)&cpu_ctxt_desc;
    io_regs.r3 = 0;

    io_regs = __arch_sysio_call(io_regs);

    return (int)io_regs.r3;
}

