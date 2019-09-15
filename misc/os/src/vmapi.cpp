
#include "vmapi.h"




ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::init ()
{
    return VMINIT();
}
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::start ()
{
    return VMBOOT();
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::restart ()
{
    arch_sysio_arg_t arg = {0, 0, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::sleep (UINT_T delay)
{
    arch_sysio_arg_t arg = {VMAPI_SLEEP, delay, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::yield (void)
{
    arch_sysio_arg_t arg = {VMAPI_YIELD, 0, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::create (THREAD_HANDLE *th)
{
    arch_sysio_arg_t arg = {VMAPI_CREATE, (WORD_T)th, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::create (_CALLBACK callback, const char *name, WORD_T stack, WORD_T prio, WORD_T size, void *arg)
{
    static THREAD_HANDLE th;
    th.Callback = callback;
    th.Name = name;
    th.Priority = prio;
    th.StackSize = stack;
    th.argSize = size;
    th.Arg = arg;
    return vm::create(&th);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::call (_CALLBACK callback, const char *name, WORD_T stack, WORD_T prio, WORD_T size, void *arg)
{
    static THREAD_HANDLE th;
    th.Callback = callback;
    th.Name = name;
    th.Priority = prio;
    th.StackSize = stack;
    th.argSize = size;
    th.Arg = arg;
    arch_sysio_arg_t __arg = {VMAPI_CALL, (WORD_T)&th, 0, 0};
    return upcall(__arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::create_drv (DRIVER_HANDLER *dh)
{
    arch_sysio_arg_t arg = {VMAPI_CREATE_DRV, (WORD_T)dh, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::lock (UINT_T id)
{
    arch_sysio_arg_t arg = {VMAPI_LOCK, id, 0, 0};
    return upcall(arg);
}    
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::unlock (UINT_T id)
{
    arch_sysio_arg_t arg = {VMAPI_UNLOCK, id, 0, 0};
    return upcall(arg);
}  

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::notify (const char *name)
{
    arch_sysio_arg_t arg = {VMAPI_NOTIFY, (WORD_T)name, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::wait_notify ()
{
    arch_sysio_arg_t arg = {VMAPI_WAIT_NOTIFY, 0, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::notify_wait (const char *name)
{
    arch_sysio_arg_t arg = {VMAPI_NOTIFY_WAIT, (WORD_T)name, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::sync (const char *name)
{
    arch_sysio_arg_t arg = {VMAPI_SYNC, (WORD_T)name, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::wait (THREAD_COND_T cond)
{
    arch_sysio_arg_t arg = {VMAPI_WAIT, (WORD_T)cond, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::wait_event (const char *event_name)
{
    arch_sysio_arg_t arg = {VMAPI_WAIT_EVENT, (WORD_T)event_name, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::fire_event (const char *event_name)
{
    arch_sysio_arg_t arg = {VMAPI_FIRE_EVENT, (WORD_T)event_name, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::mail (char *name, MAIL_HANDLE *mail)
{
    arch_sysio_arg_t arg = {VMAPI_MAIL, (WORD_T)name, (WORD_T)mail, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::wait_mail ()
{
    arch_sysio_arg_t arg = {VMAPI_WAIT_MAIL, 0, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::timer (WORD_T *dest, WORD_T id)
{
    arch_sysio_arg_t arg = {VMAPI_TIMER_CREATE, WORD_T(dest), id, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::timer_remove (WORD_T id)
{
    arch_sysio_arg_t arg = {VMAPI_TIMER_REMOVE, id, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::critical ()
{
    arch_sysio_arg_t arg = {VMAPI_CRITICAL, 0, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::end_critical ()
{
    arch_sysio_arg_t arg = {VMAPI_END_CRITICAL, 0, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::exit (UINT_T ret)
{
    arch_sysio_arg_t arg = {VMAPI_EXIT, ret, 0, 0};
    return upcall(arg);
}

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t vm::fault (const char *cause)
{
    arch_sysio_arg_t arg = {VMAPI_FAULT, (WORD_T)cause, 0, 0};
    return upcall(arg);
}

void vm::reset ()
{
    arch_sysio_arg_t arg = {VMAPI_RESET, 0, 0, 0};
    upcall(arg);
}


INT_T VMAPI_ErrorHandler (WORD_T from, ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t arg)
{
    _UNUSED(from);
    _UNUSED(arg);
    for (;;) {}
    //return 0;
}

/*End of file*/

