                 PRESERVE8
                 THUMB

                 AREA    |.text|, CODE, READONLY

Reset_Handler    PROC
                 EXPORT  Reset_Handler             [WEAK]
                 IMPORT __main
                 IMPORT system_ctor

                 BL     system_ctor
                 BL     __main

                 ENDP
                 
                 ALIGN
                     
                 EXPORT  __user_initial_stackheap
__user_initial_stackheap
                 MOV    R0, R1
                 MOV    R2, R1
                 BX     LR
                 ALIGN
                 END