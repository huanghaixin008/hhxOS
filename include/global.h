/* EXTERN is defined as extern except in global.c */
#ifdef	_HHX008_GLOBAL_H
#undef	EXTERN
#define	EXTERN
#endif

EXTERN	int		ticks;
EXTERN	int		disp_pos;
EXTERN	u8		gdt_ptr[6];	/* 0~15:Limit  16~47:Base */
EXTERN	DESCRIPTOR	gdt[GDT_SIZE];
EXTERN	u8		idt_ptr[6];	/* 0~15:Limit  16~47:Base */
EXTERN	GATE		idt[IDT_SIZE];

EXTERN u32	k_reenter;
EXTERN int	key_pressed;

EXTERN irq_handler irq_table[];

// processes table
EXTERN	PROCESS	proc_table[];
EXTERN 	TASK 	task_table[];
EXTERN 	TASK 	user_proc_table[];
extern	char		task_stack[];
EXTERN	TSS		tss;

EXTERN	PROCESS* p_proc_ready;

// tty table
EXTERN	TTY	tty_table[];
EXTERN	CONSOLE	console_table[];
EXTERN	int	nr_current_console;

/* FS */
EXTERN	struct file_desc	f_desc_table[NR_FILE_DESC];
EXTERN	struct inode		inode_table[NR_INODE];
EXTERN	struct super_block	super_block[NR_SUPER_BLOCK];
extern	u8 *			fsbuf;
extern	const int		FSBUF_SIZE;
EXTERN	MESSAGE			fs_msg;
EXTERN	PROCESS *		pcaller;
EXTERN	struct inode *		root_inode;
extern	struct dev_drv_map	dd_map[];

// MM
EXTERN	MESSAGE			mm_msg;
extern	u8 *			mmbuf;
extern	const int		MMBUF_SIZE;
EXTERN	int			memory_size;
