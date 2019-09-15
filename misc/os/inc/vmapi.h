#ifndef VMAPI_INTERFACE
#define VMAPI_INTERFACE

#include "machM4.h"
#include "thread.h"
#include "mail.h"
#include "drv.h"
#include "vmapi_call.h"


_EXTERN "C" ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t upcall (arch_sysio_arg_t a);
_EXTERN "C" ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t VMBOOT();
_EXTERN ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t VMINIT();

_WEAK INT_T VMAPI_ErrorHandler (WORD_T from, ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t arg);


namespace vm {

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t init ();
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t start (); 
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t restart (); 
    
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t sleep (UINT_T delay);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t yield (void);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t create (THREAD_HANDLE *th);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t create (_CALLBACK callback, const char *name, WORD_T stack, WORD_T prio, WORD_T size, void *arg);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t call (_CALLBACK callback, const char *name, WORD_T stack, WORD_T prio, WORD_T size, void *arg);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t create_drv (DRIVER_HANDLER *dh);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t lock (UINT_T id);   
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t unlock (UINT_T id);      
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t notify (const char *name);   
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t wait_notify ();  
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t notify_wait (const char *name);   
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t sync (const char *name);       
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t wait (THREAD_COND_T cond);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t wait_event (const char *event_name);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t fire_event (const char *event_name);   
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t mail (char *name, MAIL_HANDLE *mail);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t wait_mail ();
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t timer (WORD_T *dest, WORD_T id);
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t timer_remove (WORD_T id);

ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t critical ();
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t end_critical ();
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t exit (UINT_T ret);   
ARCH_VAL_IN_REGS_ATTR arch_sysio_arg_t fault (const char *cause); 
void reset (); 
  
};


#define _XCALL(ret, callback, size, arg) \
WORD_T ret; \
do { \
    arch_sysio_arg_t callback##_ret = vm::call(callback, #callback, VM_DEF_THREAD_HEAP_SIZE, VM_THREAD_DEF_PRIORITY, size, arg); \
    if (callback##_ret.ERROR != VM_OK) { \
        VMAPI_ErrorHandler(VMAPI_CALL, callback##_ret); \
    } else { \
        \
    } \
    ret = callback##_ret.POINTER; \
    _UNUSED(ret); \
} while (0)

#define __XCALL(ret, stack, callback, size, arg) \
WORD_T ret; \
do { \
    arch_sysio_arg_t callback##_ret = vm::call(callback, #callback, stack, VM_THREAD_DEF_PRIORITY, size, arg); \
    if (callback##_ret.ERROR != VM_OK) { \
        VMAPI_ErrorHandler(VMAPI_CALL, callback##_ret); \
    } else { \
        \
    } \
    ret = callback##_ret.POINTER; \
    _UNUSED(ret); \
} while (0)

  

#endif


/*End of file*/


