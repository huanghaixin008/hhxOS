
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            global.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define _HHX008_GLOBAL_H

#include "const.h"
#include "protect.h"
#include "proc.h"
#include "proto.h"
#include "global.h"

PROCESS	proc_table[NR_TASKS + NR_PROCS];
TASK	task_table[NR_TASKS] = {{task_tty, STACK_SIZE_TESTA, "TASK_TTY"},
				{task_sys, STACK_SIZE_TESTA, "TASK_SYS"},
				{task_hd, STACK_SIZE_TESTA, "TASK_HD"},
				{task_fs, STACK_SIZE_TESTA, "TASK_FS"},
				{task_mm, STACK_SIZE_TESTA, "TASK_MM"}};
TASK	user_proc_table[NR_PROCS] = {{Init, STACK_SIZE_TESTA, "INIT"},
				     {TestA, STACK_SIZE_TESTA, "PROC_TESTA"},
				     {TestB, STACK_SIZE_TESTB, "PROC_TESTB"},
				     {TestC, STACK_SIZE_TESTC, "PROC_TESTC"}};
char	task_stack[STACK_SIZE_TOTAL];

irq_handler irq_table[NR_IRQ];
system_call sys_call_table[NR_SYS_CALL] = {sys_sendrec, sys_printx};

TTY	tty_table[NR_CONSOLES];
CONSOLE	console_table[NR_CONSOLES];

struct dev_drv_map dd_map[] = {
	/* DRIVER NR		major device nr. */
	{INVALID_DRIVER}, 	// 0 : unused
	{INVALID_DRIVER},	// 1 : reserved for floppy driver
	{INVALID_DRIVER},	// 2 : reserved for cdrom driver
	{TASK_HD},		// 3 : Hard disk
	{TASK_TTY},		// 4 : TTY
	{INVALID_DRIVER}	// 5 : Reserved for scsi disk driver
};

/**
 * 6MB~7MB: buffer for FS
 */
u8 *		fsbuf		= (u8*)0x600000;
const int	FSBUF_SIZE	= 0x100000;

/**
 * 7MB~8MB: buffer for MM
 */
u8 *		mmbuf		= (u8*)0x700000;
const int	MMBUF_SIZE	= 0x100000;

int 	key_pressed = 0;


