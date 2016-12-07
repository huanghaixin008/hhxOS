# include "const.h"
# include "protect.h"
# include "proc.h"
# include "proto.h"
# include "global.h"

int kernel_main() {
	disp_str_os("kernel_main() begins\n");
	// init process info
	TASK*		p_task;
	PROCESS*	p_proc		= proc_table;
	char*		p_task_stack	= task_stack + STACK_SIZE_TOTAL;
	u16		selector_ldt	= SELECTOR_LDT_FIRST;

	int i, j, eflags, prior;
        u8              privilege;
        u8              rpl;

	for (i = 0; i < NR_TASKS+NR_PROCS; i++, p_proc++) {
		if (i >= NR_TASKS + NR_NATIVE_PROCS) {
			p_proc->p_flags = FREE_SLOT;
			continue;
		}

                if (i < NR_TASKS) {     /* 任务 */
                        p_task    = task_table + i;
                        privilege = PRIVILEGE_TASK;
                        rpl       = RPL_TASK;
                        eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
			prior = 50;
                }
                else {                  /* 用户进程 */
                        p_task    = user_proc_table + (i - NR_TASKS);
                        privilege = PRIVILEGE_USER;
                        rpl       = RPL_USER;
                        eflags    = 0x202; /* IF=1, bit 2 is always 1 */
			prior = 10;
                }

		strcpy(p_proc->p_name, p_task->name);	// name of the process
		p_proc->parent = NO_TASK;

		if (strcmp(p_task->name, "INIT") != 0) {
			p_proc->ldts[INDEX_LDT_C]	=	gdt[SELECTOR_KERNEL_CS >> 3];
			p_proc->ldts[INDEX_LDT_RW]	=	gdt[SELECTOR_KERNEL_DS >> 3];

			p_proc->ldts[INDEX_LDT_C].attr1 =	DA_C	| privilege << 5;
			p_proc->ldts[INDEX_LDT_RW].attr1 = 	DA_DRW	| privilege << 5;
		} else {
			unsigned int k_base;
			unsigned int k_limit;
			int ret = get_kernel_map(&k_base, &k_limit);
			assert(ret == 0);
			init_descriptor(&p_proc->ldts[INDEX_LDT_C], 0, (k_base + k_limit) >> LIMIT_4K_SHIFT, DA_32 | DA_LIMIT_4K | DA_C | privilege << 5);
			init_descriptor(&p_proc->ldts[INDEX_LDT_RW], 0, (k_base + k_limit) >> LIMIT_4K_SHIFT, DA_32 | DA_LIMIT_4K | DA_DRW | privilege << 5);
		}
		
		p_proc->p_flags = RUNNING;
		p_proc->pid = i;			// pid
		p_proc->ticks = p_proc->priority = prior;
		p_proc->p_msg = 0;
		p_proc->p_recvfrom = NO_TASK;
		p_proc->p_sendto = NO_TASK;
		p_proc->has_int_msg = 0;
		p_proc->q_sending = 0;
		p_proc->next_sending = 0;
/*
		p_proc->ldt_sel = selector_ldt;

		memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[0].attr1 = DA_C | privilege << 5;
		memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
*/
		p_proc->regs.cs	= (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ds	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.es	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.fs	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ss	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

		p_proc->regs.eip = (u32)p_task->initial_eip;
		p_proc->regs.esp = (u32)p_task_stack;
		p_proc->regs.eflags = eflags;

		p_proc->nr_tty = 0;

		for (j = 0;j < NR_FILES;j++)
			p_proc->flip[j] = 0;

		p_task_stack -= p_task->stacksize;
		selector_ldt += 1 << 3;
	}

	proc_table[NR_TASKS].nr_tty = 1;
	proc_table[NR_TASKS+1].nr_tty = 1;
	proc_table[NR_TASKS+2].nr_tty = 1;
	proc_table[NR_TASKS+3].nr_tty = 2;
	
	k_reenter = 0;
	ticks = 0;
	
	p_proc_ready = proc_table;

	init_clock();
        init_keyboard();

	restart();

	while (1) {}
}

void init_clock()
{
        /* 初始化 8253 PIT */
        out_byte(TIMER_MODE, RATE_GENERATOR);
        out_byte(TIMER0, (u8) (TIMER_FREQ/HZ) );
        out_byte(TIMER0, (u8) ((TIMER_FREQ/HZ) >> 8));

        put_irq_handler(CLOCK_IRQ, clock_handler);    /* 设定时钟中断处理程序 */
        enable_irq(CLOCK_IRQ);                        /* 让8259A可以接收时钟中断 */
}

/**
 * @struct posix_tar_header
 * Borrowed from GNU `tar'
 */
struct posix_tar_header
{				/* byte offset */
	char name[100];		/*   0 */
	char mode[8];		/* 100 */
	char uid[8];		/* 108 */
	char gid[8];		/* 116 */
	char size[12];		/* 124 */
	char mtime[12];		/* 136 */
	char chksum[8];		/* 148 */
	char typeflag;		/* 156 */
	char linkname[100];	/* 157 */
	char magic[6];		/* 257 */
	char version[2];	/* 263 */
	char uname[32];		/* 265 */
	char gname[32];		/* 297 */
	char devmajor[8];	/* 329 */
	char devminor[8];	/* 337 */
	char prefix[155];	/* 345 */
	/* 500 */
};

/*****************************************************************************
 *                                untar
 *****************************************************************************/
/**
 * Extract the tar file and store them.
 * 
 * @param filename The tar file.
 *****************************************************************************/
void untar(const char * filename)
{
	printf("[extract `%s'...\n", filename);
	int fd = open(filename, O_RDWR);
	assert(fd != -1);

	char buf[SECTOR_SIZE * 16];
	int chunk = sizeof(buf);

	while (1) {
		read(fd, buf, SECTOR_SIZE);
		if (buf[0] == 0)
			break;

		struct posix_tar_header * phdr = (struct posix_tar_header *)buf;

		/* calculate the file size */
		char * p = phdr->size;
		int f_len = 0;
		while (*p)
			f_len = (f_len * 8) + (*p++ - '0'); /* octal */

		int bytes_left = f_len;
		int fdout = open(phdr->name, O_CREAT | O_RDWR | O_TRUNC);
		if (fdout == -1) {
			printf("    failed to extract file: %s\n", phdr->name);
			printf(" aborted]");
			return;
		}
		printf("    %s (%d bytes): ", phdr->name, f_len);
		while (bytes_left) {
			int iobytes = min(chunk, bytes_left);
			read(fd, buf,
			     ((iobytes - 1) / SECTOR_SIZE + 1) * SECTOR_SIZE);
			write(fdout, buf, iobytes);
			bytes_left -= iobytes;
			printf("*");
		}
		printf("\n");
		close(fdout);
	}
	close(fd);

	printf(" done]\n");
}

void Init()
{	
	int i = 3;
	while (i--) {
		printl("p...");
		delay(1);
	}
	printl("\n");

	int fd_stdin = open("/dev_tty1", O_RDWR);
	assert(fd_stdin == 0);
	int fd_stdout = open("/dev_tty1", O_RDWR);
	assert(fd_stdout == 1);

	printf("Init() is running... \n");

	/* extract `cmd.tar' */
	untar("/cmd.tar");

	int pid = fork();
	if (pid != 0) {
		printf("parent is running, child pid: %d\n", pid);
		//spin("parent");
		while (1) {
			int s;		
			int child = wait(&s);
			printf("child (%d) exited with status: %d\n", child, s);
		}
	} else {
		printf("child is running, pid: %d\n", getpid());
		//spin("children");
		close(fd_stdin);
		close(fd_stdout);

		simple_shell("/dev_tty2");
		assert(0);
		//execl("/echo", "echo", "hello", "world", 0);
		//exit(123);
	}

}

void TestA() {
	int i = 2;
	while (1) {
		//printl("p...");
		//delay(1);
	}
	printl("\n");

	char tty_name[] = "/dev_tty1";
	int fd_stdin = open(tty_name, O_RDWR);
	assert(fd_stdin == 0);
	int fd_stdout = open(tty_name, O_RDWR);
	assert(fd_stdout == 1);

	int fd, n;
	const char filename[] = "blah";
	const char bufw[] = "abcdef";
	const int rd_bytes = 3;
	char buffer[rd_bytes];
	char bufr[rd_bytes];

	assert(rd_bytes <= strlen(bufw));

	fd = open("/blah", O_CREAT | O_RDWR);
	assert(fd != -1);
	printf("File created fd: %d\n", fd);

	n = write(fd, bufw, strlen(bufw));
	assert(n == strlen(bufw));
	
	close(fd);

	fd = open("/blah", O_RDWR);
	assert(fd != -1);
	printf("File opened fd: %d\n", fd);

	n = read(fd, bufr, rd_bytes);
	assert(n == rd_bytes);
	bufr[n] = 0;
	printf("%d bytes read: %s\n", n, bufr);
	close(fd);

	const char* filename2[] = {"/foo", "/bar", "/baz", "/dev_tty0"};
	for (i = 0; i < 3;i++) {
		fd = open(filename2[i], O_CREAT | O_RDWR);
		assert(fd != -1);
		printf("File created: %s (fd %d)\n", filename2[i], fd);
		close(fd);
	}

	for (i = 0; i < 4;i++) {
		if (unlink(filename2[i]) == 0)
			printf("File removed: %s\n", filename2[i]);
		else
			printf("Failed to remove file: %s\n", filename2[i]);
	}

	while (1) {
		printf("<A>");
		delay(1);
	}
}

void TestB() {
	int i = 0x1000;
	while (1) {
		//disp_str("B");
		//printl("<B>");
		delay(1);
		//milli_delay(10000);
	}
}

void TestC() {
	while (1) {
		delay(1);
	}
/*
	int i = 2;
	while (i--) {
		printl("p...");
		delay(1);
	}
	printl("\n");	
	
	char tty_name[] = "/dev_tty2";
	int fd_stdin = open(tty_name, O_RDWR);
	assert(fd_stdin == 0);
	int fd_stdout = open(tty_name, O_RDWR);
	assert(fd_stdout == 1);

	char rdbuf[128];

	while (1) {
		write(fd_stdout, "$ ", 2);
		int r = read(fd_stdin, rdbuf, 70);
		rdbuf[r] = '\0';

		if (strcmp(rdbuf, "hello") == 0)
			printf("hello_world!\n");
		else if (rdbuf[0])
			printf("{%s}\n", rdbuf);
	}
*/
}

void simple_shell(const char * tty_name) {
	int fd_stdin = open(tty_name, O_RDWR);
	assert(fd_stdin == 0);
	int fd_stdout = open(tty_name, O_RDWR);
	assert(fd_stdout == 1);

	char rdbuf[128];

	while (1) {
		write(fd_stdout, "$ ", 2);
		int r = read(fd_stdin, rdbuf, 70);
		rdbuf[r] = 0;

		int argc = 0;
		char * argv[PROC_ORIGIN_STACK];
		char * p = rdbuf;
		char * s;
		int word = 0;
		char ch;
		do {
			ch = *p;
			if (*p != ' ' && *p != 0 && !word) {
				s = p;
				word = 1;
			}
			if ((*p == ' ' || *p == 0) && word) {
				word = 0;
				argv[argc++] = s;
				*p = 0;
			}
			p++;
		} while (ch);
		argv[argc] = 0;
		
		if (strcmp(rdbuf, "hello") == 0) {
			write(fd_stdout, "hello_world!\n", strlen("hello_world!\n"));
			continue;
		}

		int fd = open(argv[0], O_RDWR);
		if (fd == -1) {
			if (rdbuf[0]) {
				write(fd_stdout, "{", 1);
				write(fd_stdout, rdbuf, r);
				write(fd_stdout, "}\n", 2);
			}
		} else {
			close(fd);
			int pid = fork();
			if (pid != 0) {
				int s;
				wait(&s);
				printf("{Done executing with return status: %d}\n", s);
			} else execv(argv[0], argv);
		}
	}
}

int get_ticks() {
	MESSAGE msg;
	reset_msg(&msg);
	msg.type = GET_TICKS;
	send_recv(BOTH, TASK_SYS, &msg);
	return msg.RETVAL;
}

void panic(const char*fmt, ...)
{
	int i;
	char buf[256];

	va_list arg = (va_list)((char*)&fmt + 4);
	i = vsprintf(buf, fmt, arg);

	printl("%c !!panic!! %s", MAG_CH_PANIC, buf);
}
