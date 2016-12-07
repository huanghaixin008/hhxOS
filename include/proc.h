// for process
#ifndef _HHX008_PROC_H
#define _HHX008_PROC_H

# define PROCS_BASE	0xA00000
# define PROC_IMAGE_SIZE_DEFAULT	0x100000
# define PROC_ORIGIN_STACK	0x400

typedef struct s_stackframe {
	u32	gs;		/* \                                    */
	u32	fs;		/* |                                    */
	u32	es;		/* |                                    */
	u32	ds;		/* |                                    */
	u32	edi;		/* |                                    */
	u32	esi;		/* | pushed by save()                   */
	u32	ebp;		/* |                                    */
	u32	kernel_esp;	/* <- 'popad' will ignore it            */
	u32	ebx;		/* |                                    */
	u32	edx;		/* |                                    */
	u32	ecx;		/* |                                    */
	u32	eax;		/* /                                    */
	u32	retaddr;	/* return addr for kernel.asm::save()   */
	u32	eip;		/* \                                    */
	u32	cs;		/* |                                    */
	u32	eflags;		/* | pushed by CPU during interrupt     */
	u32	esp;		/* |                                    */
	u32	ss;		/* /                                    */
} STACK_FRAME;

struct mess1 {
	int m1i1;
	int m1i2;
	int m1i3;
	int m1i4;
};
struct mess2 {
	void* m2p1;
	void* m2p2;
	void* m2p3;
	void* m2p4;
};
struct mess3 {
	int	m3i1;
	int	m3i2;
	int	m3i3;
	int	m3i4;
	u64	m3l1;
	u64	m3l2;
	void*	m3p1;
	void*	m3p2;
};
typedef struct {
	int source;
	int type;
	union {
		struct mess1 m1;
		struct mess2 m2;
		struct mess3 m3;
	} u;
} MESSAGE;

typedef struct s_proc {
	STACK_FRAME regs;
	
	u16 ldt_sel;
	DESCRIPTOR ldts[LDT_SIZE];

	int ticks;
	int priority;

	u32 pid;
	char p_name[16];

	int p_flags;

	MESSAGE * p_msg;
	int p_recvfrom;
	int p_sendto;

	int has_int_msg;
	struct s_proc* q_sending;
	struct s_proc* next_sending;
	
	int nr_tty;
	int parent; /**< pid of parent process */

	int exit_status; /**< for parent */

	struct file_desc * flip[NR_FILES];
} PROCESS;

typedef void (*task_f) ();

typedef struct s_task {
	task_f initial_eip;
	int stacksize;
	char name[16];
} TASK;

#define proc2pid(x) (x - proc_table)

#endif
