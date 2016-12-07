# include <const.h>
# include <protect.h>
# include <proc.h>
# include <proto.h>
# include <elf.h>
# include <global.h>

static void init_mm();
static void killproc(int pid);

void task_mm()
{
	init_mm();

	while (1) {
		send_recv(RECEIVE, ANY, &mm_msg);
		int src = mm_msg.source;
		int reply = 1;

		int msgtype = mm_msg.type;

		switch (msgtype) {
		case FORK:
			mm_msg.RETVAL = do_fork();
			break;
		case EXEC:
			mm_msg.RETVAL = do_exec();
			break;
		case EXIT:
			do_exit(mm_msg.STATUS);
			reply = 0;
			break;
		case WAIT:
			do_wait();
			reply = 0;
			break;
		default:
			printl("{MM} dump, unknown message: %d\n", msgtype);
			break;
		}

		if (reply) {
			mm_msg.type = SYSCALL_RET;
			send_recv(SEND, src, &mm_msg);
		}
	}
}

static void init_mm()
{
	struct boot_params bp;
	get_boot_params(&bp);

	memory_size = bp.mem_size;

	printl("{MM} memsize: %dMB\n", memory_size / (1024*1024));
}

int do_fork()
{
	PROCESS* p = proc_table;
	int i;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++, p++)
		if (p->p_flags == FREE_SLOT)
			break;

	if (i >= NR_TASKS + NR_PROCS)
		return -1;
	assert(i < NR_TASKS + NR_PROCS);

	int child_pid = i;
	assert(p == &proc_table[child_pid]);
	assert(child_pid >= NR_TASKS + NR_NATIVE_PROCS);

	int pid = mm_msg.source;
	u16 child_ldt_sel = p->ldt_sel;
	*p = proc_table[pid];
	p->ldt_sel = child_ldt_sel;
	p->parent = pid;
#ifdef DEBUG
	printl("forking: %s_%d; ", proc_table[pid].p_name, child_pid);
#endif

	DESCRIPTOR * ppd;

	/* Text segment */
	ppd = &proc_table[pid].ldts[INDEX_LDT_C];
	/* base of T-seg, in bytes */
	int caller_T_base  = reassembly(ppd->base_high, 24,
					ppd->base_mid,  16,
					ppd->base_low);
	/* limit of T-seg, in 1 or 4096 bytes,
	   depending on the G bit of descriptor */
	int caller_T_limit = reassembly(0, 0,
					(ppd->limit_high_attr2 & 0xF), 16,
					ppd->limit_low);
	/* size of T-seg, in bytes */
	int caller_T_size  = ((caller_T_limit + 1) *
			      ((ppd->limit_high_attr2 & (DA_LIMIT_4K >> 8)) ?
			       4096 : 1));

	/* Data & Stack segments */
	ppd = &proc_table[pid].ldts[INDEX_LDT_RW];
	/* base of D&S-seg, in bytes */
	int caller_D_S_base  = reassembly(ppd->base_high, 24,
					  ppd->base_mid,  16,
					  ppd->base_low);
	/* limit of D&S-seg, in 1 or 4096 bytes,
	   depending on the G bit of descriptor */
	int caller_D_S_limit = reassembly((ppd->limit_high_attr2 & 0xF), 16,
					  0, 0,
					  ppd->limit_low);
	/* size of D&S-seg, in bytes */
	int caller_D_S_size  = ((caller_T_limit + 1) *
				((ppd->limit_high_attr2 & (DA_LIMIT_4K >> 8)) ?
				 4096 : 1));

	/* we don't separate T, D & S segments, so we have: */
	assert((caller_T_base  == caller_D_S_base ) &&
	       (caller_T_limit == caller_D_S_limit) &&
	       (caller_T_size  == caller_D_S_size ));

	int child_base = alloc_mem(child_pid, caller_T_size);
#ifdef DEBUG
	printl("{MM} 0x%x <- 0x%x (0x%x bytes)\n",
	       child_base, caller_T_base, caller_T_size);
#endif

	phys_copy((void*)child_base, (void*)caller_T_base, caller_T_size);

	init_descriptor(&p->ldts[INDEX_LDT_C], child_base, (PROC_IMAGE_SIZE_DEFAULT - 1) >> LIMIT_4K_SHIFT,
			DA_LIMIT_4K | DA_32 | DA_C | PRIVILEGE_USER << 5);
	init_descriptor(&p->ldts[INDEX_LDT_RW], child_base, (PROC_IMAGE_SIZE_DEFAULT - 1) >> LIMIT_4K_SHIFT,
			DA_LIMIT_4K | DA_32 | DA_DRW | PRIVILEGE_USER << 5);

	MESSAGE msg2fs;
	msg2fs.type = FORK;
	msg2fs.PID = child_pid;
	send_recv(BOTH, TASK_FS, &msg2fs);

	mm_msg.PID = child_pid;

	MESSAGE m;
	m.type = SYSCALL_RET;
	m.RETVAL = 0;
	m.PID = 0;
	send_recv(SEND, child_pid, &m);

	return 0;
}

int do_exec()
{
	/* get parameters from the message */
	int name_len = mm_msg.NAME_LEN;	/* length of filename */
	int src = mm_msg.source;	/* caller proc nr. */
	assert(name_len < MAX_PATH);

	char pathname[MAX_PATH];
	phys_copy((void*)va2la(TASK_MM, pathname),
		  (void*)va2la(src, mm_msg.PATHNAME),
		  name_len);
	pathname[name_len] = 0;	/* terminate the string */

	/* get the file size */
	struct stat s;
	int ret = stat(pathname, &s);
	if (ret != 0) {
		printl("{MM} MM::do_exec()::stat() returns error. %s", pathname);
		return -1;
	}

	/* read the file */
	int fd = open(pathname, O_RDWR);
	if (fd == -1)
		return -1;
	assert(s.st_size < MMBUF_SIZE);
	read(fd, mmbuf, s.st_size);
	close(fd);

	/* overwrite the current proc image with the new one */
	Elf32_Ehdr* elf_hdr = (Elf32_Ehdr*)(mmbuf);
	int i;
	for (i = 0; i < elf_hdr->e_phnum; i++) {
		Elf32_Phdr* prog_hdr = (Elf32_Phdr*)(mmbuf + elf_hdr->e_phoff +
			 			(i * elf_hdr->e_phentsize));
		if (prog_hdr->p_type == PT_LOAD) {
			assert(prog_hdr->p_vaddr + prog_hdr->p_memsz <
				PROC_IMAGE_SIZE_DEFAULT);
			phys_copy((void*)va2la(src, (void*)prog_hdr->p_vaddr),
				  (void*)va2la(TASK_MM,
						 mmbuf + prog_hdr->p_offset),
				  prog_hdr->p_filesz);
		}
	}

	/* setup the arg stack */
	int orig_stack_len = mm_msg.BUF_LEN;
	char stackcopy[PROC_ORIGIN_STACK];
	phys_copy((void*)va2la(TASK_MM, stackcopy),
		  (void*)va2la(src, mm_msg.BUF),
		  orig_stack_len);

	u8 * orig_stack = (u8*)(PROC_IMAGE_SIZE_DEFAULT - PROC_ORIGIN_STACK);

	int delta = (int)orig_stack - (int)mm_msg.BUF;

	int argc = 0;
	if (orig_stack_len) {	/* has args */
		char **q = (char**)stackcopy;
		for (; *q != 0; q++,argc++)
			*q += delta;
	}

	phys_copy((void*)va2la(src, orig_stack),
		  (void*)va2la(TASK_MM, stackcopy),
		  orig_stack_len);

	proc_table[src].regs.ecx = argc; /* argc */
	proc_table[src].regs.eax = (u32)orig_stack; /* argv */

	/* setup eip & esp */
	proc_table[src].regs.eip = elf_hdr->e_entry; /* @see _start.asm */
	proc_table[src].regs.esp = PROC_IMAGE_SIZE_DEFAULT - PROC_ORIGIN_STACK;

	strcpy(proc_table[src].p_name, pathname);

	return 0;
}

void do_wait()
{
	int pid = mm_msg.source;

	int i;
	int children = 0;
	for (i = 0;i < NR_TASKS + NR_PROCS; i++) {
		if (proc_table[i].parent == pid)
			if (proc_table[i].p_flags & HANGING) {
				killproc(i);
				return;
			}
			children++;
	}

	if (children > 0) {
		proc_table[pid].p_flags |= WAITING;
	} else {
		MESSAGE msg;
		msg.type = SYSCALL_RET;
		msg.PID = NO_TASK;
		send_recv(SEND, pid, &msg);
	}
}

void do_exit(int status)
{
	int pid = mm_msg.source;
	
	MESSAGE msg2fs;
	msg2fs.type = EXIT;
	msg2fs.PID = pid;
	proc_table[pid].exit_status = status;
	send_recv(BOTH, TASK_FS, &msg2fs);

	free_mem(pid);
	
	int parent = proc_table[pid].parent;
	if (proc_table[parent].p_flags & WAITING) {
		proc_table[parent].p_flags &= ~WAITING;
		killproc(pid);
	} else proc_table[pid].p_flags |= HANGING;
	int i;
	for (i = 0;i<NR_TASKS + NR_PROCS;i++) {
		if (proc_table[i].parent == pid) {
			proc_table[i].parent = PID_INIT;
			if (proc_table[i].p_flags & HANGING && proc_table[PID_INIT].p_flags & WAITING) {
				proc_table[PID_INIT].p_flags &= ~WAITING;
				killproc(i);
			}
		}
	}
}

static void killproc(int pid)
{
	PROCESS* p = &proc_table[pid];

	MESSAGE msg2parent;
	msg2parent.type = SYSCALL_RET;
	msg2parent.PID = proc2pid(p);
	msg2parent.STATUS = p->exit_status;
	send_recv(SEND, p->parent, &msg2parent);

	p->p_flags = FREE_SLOT;
}

int alloc_mem(int pid, int memsize)
{
	assert(pid >= (NR_TASKS + NR_NATIVE_PROCS));
	if (memsize > PROC_IMAGE_SIZE_DEFAULT) {
		panic("unsupported memory request: %d."
			"(should be less than %d)",
			memsize, PROC_IMAGE_SIZE_DEFAULT);
	}

	int base = PROCS_BASE + (pid - (NR_TASKS + NR_NATIVE_PROCS)) * PROC_IMAGE_SIZE_DEFAULT;

	if (base + memsize >= memory_size)
		panic("memory allocation failed, pid:%d", pid);
	
	return base;
}

int free_mem(int pid)
{
	return 0; 
}
