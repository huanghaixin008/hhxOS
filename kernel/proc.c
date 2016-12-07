# include <const.h>
# include <protect.h>
# include <proc.h>
# include <proto.h>
# include <global.h>

static void block(PROCESS* p);
static void unblock(PROCESS* p);
static int deadlock(int src, int dest);
static int msg_send(PROCESS* current, int dest, MESSAGE* m);
static int msg_receive(PROCESS* current, int src, MESSAGE* m);

void clock_handler(int irq) {

	if (key_pressed)
		inform_int(TASK_TTY);
	//disp_str("#");
	ticks++;
	p_proc_ready->ticks--;

	if (k_reenter != 0)
		return;

	if (p_proc_ready->ticks > 0)
		return;
//p_proc_ready = proc_table + k_taskid;
	schedule();
}

void schedule() {
	PROCESS* p;
	int max_ticks = 0;

	while (!max_ticks) {
		for (p=proc_table;p<proc_table+NR_TASKS+NR_PROCS;p++) {
			if (p->p_flags == RUNNING && p->ticks > max_ticks) {
				max_ticks = p->ticks;
				p_proc_ready = p;
			}
		}

		if (!max_ticks) {
			for (p=proc_table;p<proc_table+NR_TASKS+NR_PROCS;p++)
				if (p->p_flags == RUNNING)
					p->ticks = p->priority;
		}
	}
}

int getpid()
{
	MESSAGE msg;
	msg.type	= GET_PID;

	send_recv(BOTH, TASK_SYS, &msg);
	assert(msg.type == SYSCALL_RET);

	return msg.PID;
}

int sys_get_ticks() {
	return ticks;
}

int sys_sendrec(int function, int src_dest, MESSAGE* m, PROCESS* p) {
	assert(k_reenter == 0);
	assert((src_dest >= 0 && src_dest < NR_TASKS + NR_PROCS) || 
		src_dest == ANY ||
		src_dest == INTERRUPT);

	int ret = 0;
	int caller = proc2pid(p);
	MESSAGE* mla = (MESSAGE*)va2la(caller, m);
	mla->source = caller;
	assert(mla->source != src_dest);

	if (function == SEND) {
		ret = msg_send(p, src_dest, m);
		if (ret != 0)
			return ret;
	}
	else if (function == RECEIVE) {
		ret = msg_receive(p, src_dest, m);
		if (ret != 0)
			return ret;
	}
	else panic("{sys_sendrec} invalid function!");	
	
	return 0;
}

int send_recv(int function, int src_dest, MESSAGE* msg)
{
	int ret = 0;
	switch (function) {
	case BOTH:
		ret = sendrec(SEND, src_dest, msg);
		if (ret == 0)
			ret = sendrec(RECEIVE, src_dest, msg);
		break;
	case RECEIVE:
		memset(msg, 0, sizeof(MESSAGE));
	case SEND:
		ret = sendrec(function, src_dest, msg);
		break;
	default:
		assert((function == BOTH) || (function == SEND) || (function == RECEIVE));
		break;
	}
	
	return ret;
}

int ldt_seg_linear(PROCESS* p, int idx) 
{
	DESCRIPTOR * d = &p->ldts[idx];

	return d->base_high << 24 | d->base_mid << 16 | d->base_low; 
}

void* va2la(int pid, void* va)
{	
	PROCESS* p = &proc_table[pid];

	u32 seg_base = ldt_seg_linear(p, INDEX_LDT_RW);
	u32 la = seg_base + (u32)va;

	if (pid < NR_TASKS + NR_NATIVE_PROCS) {
		assert(la == (u32)va);
	}

	return (void*)la;
}

void reset_msg(MESSAGE* p)
{
	memset(p, 0, sizeof(MESSAGE));
}

static void block(PROCESS* p)
{
	assert(p->p_flags);
	schedule();
}

static void unblock(PROCESS* p)
{
	assert(p->p_flags == 0);
}

static int deadlock(int src, int dest)
{
	PROCESS* p = proc_table + dest;
	while (1) {
		if (p->p_flags & SENDING) {
			if (p->p_sendto == src) {
				// print the chain
				p = proc_table + dest;
				printl("===%s", p->p_name);
				do {
					assert(p->p_msg);
					p = proc_table + p->p_sendto;
				} while (p != proc_table + src);
				printl("===");

				return 1;
			}
			p = proc_table + p->p_sendto;
		} else return 0;
	}
}

static int msg_send(PROCESS* current, int dest, MESSAGE* m)
{
	PROCESS* sender = current;
	PROCESS* p_dest = proc_table + dest;
	
	assert(proc2pid(sender) != dest);

	if (deadlock(proc2pid(sender), dest))
		panic(">>DEADLOCK<< %s->%s", sender->p_name, p_dest->p_name);

	if ((p_dest->p_flags & RECEIVING) && 
		(p_dest->p_recvfrom == proc2pid(sender) || p_dest->p_recvfrom == ANY)) {
		assert(p_dest->p_msg);
		assert(m);

		phys_copy(va2la(dest, p_dest->p_msg),
			va2la(proc2pid(sender), m), sizeof(MESSAGE));
		p_dest->p_msg = 0;
		p_dest->p_flags &= ~RECEIVING;
		p_dest->p_recvfrom = NO_TASK;
		unblock(p_dest);
		
		assert(p_dest->p_flags == 0);
		assert(p_dest->p_msg == 0);
		assert(p_dest->p_recvfrom == NO_TASK);
		assert(p_dest->p_sendto == NO_TASK);
		assert(sender->p_flags == 0);
		assert(sender->p_msg == 0);
		assert(sender->p_recvfrom == NO_TASK);
		assert(sender->p_sendto == NO_TASK);
	} else {
		sender->p_flags |= SENDING;
		assert(sender->p_flags & SENDING);
		sender->p_sendto = dest;
		sender->p_msg = m;

		PROCESS* p;
		if (p_dest->q_sending) {
			p = p_dest->q_sending;
			while (p->next_sending)
				p = p->next_sending;
			p->next_sending = sender;
		} else p_dest->q_sending = sender;
		sender->next_sending = 0;

		block(sender);
		
		assert(sender->p_flags == SENDING);
		assert(sender->p_msg != 0);
		assert(sender->p_recvfrom == NO_TASK);
		assert(sender->p_sendto == dest);
	}

	return 0;
}

static int msg_receive(PROCESS* current, int src, MESSAGE* m)
{
	PROCESS* p_received = current;
	PROCESS* p_from = 0;
	PROCESS* prev = 0;
	int copyok = 0;

	assert(proc2pid(p_received) != src);

	if (p_received->has_int_msg && (src == ANY || src == INTERRUPT)) {
		MESSAGE msg;
		reset_msg(&msg);
		msg.source = INTERRUPT;
		msg.type = HARD_INT;
		assert(m);
		phys_copy(va2la(proc2pid(p_received), m), &msg, sizeof(MESSAGE));

		p_received->has_int_msg = 0;
		
		assert(p_received->p_flags == RUNNING);
		assert(p_received->p_msg == 0);
		assert(p_received->p_sendto == NO_TASK);
		assert(p_received->has_int_msg == 0);

		return 0;
	}

	if (src == ANY) {
		if (p_received->q_sending) {
			p_from = p_received->q_sending;
			copyok = 1;		

			assert(p_received->p_flags == RUNNING);
			assert(p_received->p_msg == 0);
			assert(p_received->p_recvfrom == NO_TASK);
			assert(p_received->p_sendto == NO_TASK);
			assert(p_received->q_sending != 0);
			assert(p_from->p_flags & SENDING);
			assert(p_from->p_msg != 0);
			assert(p_from->p_recvfrom == NO_TASK);
			assert(p_from->p_sendto == proc2pid(p_received));
		}
	} else {
		p_from = &proc_table[src];
		if ((p_from->p_flags & SENDING) && p_from->p_sendto == proc2pid(p_received)) {
			copyok = 1;
			PROCESS* p = p_received->q_sending;
			assert(p);

			while (p) {
				assert(p_from->p_flags & SENDING);
				if (proc2pid(p) == src) {
					p_from = p;
					break;
				}
				prev = p;
				p = p->next_sending;
			}

			assert(p_received->p_flags == RUNNING);
			assert(p_received->p_msg == 0);
			assert(p_received->p_recvfrom == NO_TASK);
			assert(p_received->p_sendto == NO_TASK);
			assert(p_received->q_sending != 0);
			assert(p_from->p_flags & SENDING);
			assert(p_from->p_msg != 0);
			assert(p_from->p_recvfrom == NO_TASK);
			assert(p_from->p_sendto == proc2pid(p_received));
		}
	}

	if (copyok) {
		if (p_from == p_received->q_sending) {
			assert(prev == 0);
			p_received->q_sending = p_from->next_sending;
			p_from->next_sending = 0;
		} else {
			assert(prev);
			prev->next_sending = p_from->next_sending;
			p_from->next_sending = 0;
		}

		assert(m);
		assert(p_from->p_msg);

		phys_copy(va2la(proc2pid(p_received), m), va2la(proc2pid(p_from), p_from->p_msg), sizeof(MESSAGE));

		p_from->p_msg = 0;
		p_from->p_sendto = NO_TASK;
		p_from->p_flags &= ~SENDING;
		unblock(p_from);
	} else {
		p_received->p_flags |= RECEIVING;
		p_received->p_msg = m;

		if (src == ANY)
			p_received->p_recvfrom = ANY;
		else p_received->p_recvfrom = proc2pid(p_from);

		block(p_received);

		assert(p_received->p_flags & RECEIVING);
		assert(p_received->p_msg != 0);
		assert(p_received->p_recvfrom != NO_TASK);
		assert(p_received->p_sendto == NO_TASK);
		assert(p_received->has_int_msg == 0);
	}

	return 0;
}

void inform_int(int task_nr)
{
	PROCESS* p = proc_table + task_nr;

	if ((p->p_flags & RECEIVING) && /* dest is waiting for the msg */
	    ((p->p_recvfrom == INTERRUPT) || (p->p_recvfrom == ANY))) {
		p->p_msg->source = INTERRUPT;
		p->p_msg->type = HARD_INT;
		p->p_msg = 0;
		p->has_int_msg = 0;
		p->p_flags &= ~RECEIVING; /* dest has received the msg */
		p->p_recvfrom = NO_TASK;
		assert(p->p_flags == RUNNING);
		unblock(p);

		assert(p->p_flags == RUNNING);
		assert(p->p_msg == 0);
		assert(p->p_recvfrom == NO_TASK);
		assert(p->p_sendto == NO_TASK);
	}
	else {
		p->has_int_msg = 1;
	}
}
