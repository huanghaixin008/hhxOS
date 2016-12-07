#ifndef _HHX008_TTY_H_
#define _HHX008_TTY_H_

# include <const.h>
# include <protect.h>
# include <proto.h>
# include <proc.h>
# include <global.h>
# include "io.h"

# define TTY_FIRST tty_table
# define TTY_LAST tty_table + NR_CONSOLES

static void tty_dev_read(TTY*);
static void tty_dev_write(TTY*);
static void tty_do_read(TTY*, MESSAGE*);
static void tty_do_write(TTY*, MESSAGE*);
static void set_cursor(unsigned int);
static void set_video_start_addr(u32);

void task_tty() {
	TTY* tty;
	MESSAGE msg;

	for (tty=TTY_FIRST;tty<=TTY_LAST;tty++)
		init_tty(tty);
	select_console(0);

	while (1) {
		for (tty=TTY_FIRST;tty<=TTY_LAST;tty++) {
			do {			
				tty_dev_read(tty);
				tty_dev_write(tty);
			} while (tty->inbuf_count);
		}
		//panic("in TTY");

		send_recv(RECEIVE, ANY, &msg);

		int src = msg.source;
		assert(src != TASK_TTY);
		
		TTY* ptty = &tty_table[msg.DEVICE];

		switch (msg.type) {
		case DEV_OPEN:
			reset_msg(&msg);
			msg.type = SYSCALL_RET;
			send_recv(SEND, src, &msg);
			break;
		case DEV_READ:
			tty_do_read(ptty, &msg);
			break;
		case DEV_WRITE:
			tty_do_write(ptty, &msg);
			break;
		case HARD_INT:
			key_pressed = 0;
			continue;
		default:
			printl("{TTY} dump, unknown message: %d\n", msg.type);
			break;
		}
	}
}

void init_tty(TTY* p_tty) {
	p_tty->inbuf_count = 0;
	p_tty->tty_left_cnt = 0;
	p_tty->p_inbuf_head = p_tty->p_inbuf_tail = p_tty->p_inbuf;
	
	init_screen(p_tty);
}

void init_screen(TTY* p_tty) {
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;

	int con_v_mem_size	=	v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr	=	nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit	=	con_v_mem_size;
	p_tty->p_console->curr_start_addr	=	p_tty->p_console->original_addr;

	p_tty->p_console->cursor = p_tty->p_console->original_addr;
	if (nr_tty == 0) {
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	} else {
		out_char(p_tty->p_console, '#');
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '\n');
	}

	set_cursor(p_tty->p_console->cursor);
}

static void tty_dev_read(TTY* p_tty) {
	if (p_tty->p_console == &console_table[nr_current_console])
		keyboard_read(p_tty);
}

static void tty_dev_write(TTY* p_tty) {
	if (p_tty->inbuf_count) {
		char ch = *(p_tty->p_inbuf_tail);
		p_tty->p_inbuf_tail++;
		if (p_tty->p_inbuf_tail == p_tty->p_inbuf + TTY_IN_BYTES)
			p_tty->p_inbuf_tail = p_tty->p_inbuf;
		p_tty->inbuf_count--;

		if (p_tty->tty_left_cnt) {
			out_char(p_tty->p_console, ch);
			if (ch >= ' ' && ch <= '~') {
				void * p = p_tty->tty_req_buf + p_tty->tty_trans_cnt;
				phys_copy(p, (void*)va2la(TASK_TTY, &ch), 1);
				p_tty->tty_trans_cnt++;
				p_tty->tty_left_cnt--;
			} else if (ch == 'b' && p_tty->tty_trans_cnt) {
				p_tty->tty_trans_cnt--;
				p_tty->tty_left_cnt++;
			}

			if (ch == '\n' || p_tty->tty_left_cnt == 0) {
				MESSAGE msg;
				msg.type = RESUME_PROC;
				msg.PROC_NR = p_tty->tty_procnr;
				msg.CNT = p_tty->tty_trans_cnt;
				send_recv(SEND, p_tty->tty_caller, &msg);
				p_tty->tty_left_cnt = 0;
			}
		}
	}
}

static void tty_do_read(TTY* p_tty, MESSAGE* msg) {
	p_tty->tty_caller	=	msg->source;
	p_tty->tty_procnr	=	msg->PROC_NR;
	p_tty->tty_req_buf	=	va2la(p_tty->tty_procnr, msg->BUF);

	p_tty->tty_left_cnt	=	msg->CNT;
	p_tty->tty_trans_cnt	=	0;

	msg->type	=	SUSPEND_PROC;
	msg->CNT	=	p_tty->tty_left_cnt;
	send_recv(SEND, p_tty->tty_caller, msg);
}

static void tty_do_write(TTY* p_tty, MESSAGE* msg) {
	char buf[TTY_OUT_BUF_LEN];
	char* p = (char*)va2la(msg->PROC_NR, msg->BUF);
	int i = msg->CNT;
	int j;

	while (i) {
		int bytes = min(TTY_OUT_BUF_LEN, i);
		phys_copy(va2la(TASK_TTY, buf), (void*)p, bytes);
		for (j = 0; j < bytes; j++)
			out_char(p_tty->p_console, buf[j]);
		i -= bytes;
		p += bytes;
	}

	msg->type = SYSCALL_RET;
	send_recv(SEND, msg->source, msg);
}

void out_char(CONSOLE* p_con, char ch) {
	u8* p_vmem = (u8*) (V_MEM_BASE + (p_con->cursor * 2));
	
	switch (ch) {
	case '\n':
		if (p_con->cursor < p_con->original_addr + p_con->v_mem_limit - SCREEN_WIDTH) {
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * ((p_con->cursor - p_con->original_addr) / SCREEN_WIDTH + 1);
		}
		break;
	case '\b':
		if (p_con->cursor > p_con->original_addr) {
			*(p_vmem-2) = ' ';
			*(p_vmem-1) = DEFAULT_CHAR_COLOR;
			p_con->cursor--;
		}
		break;
	default:
		if (p_con->cursor < p_con->original_addr + p_con->v_mem_limit - 1) {
			*p_vmem++ = ch;
			*p_vmem++ = DEFAULT_CHAR_COLOR;
			p_con->cursor++;
		}
		break;
	}

	while (p_con->cursor >= p_con->curr_start_addr + SCREEN_SIZE)
		scroll_screen(p_con, SCR_DN);

	set_cursor(p_con->cursor);
	//set_video_start_addr(p_con->curr_start_addr);
}

static void set_cursor(unsigned int pos) {
	disable_int();
	out_byte(CRTC_ADDR_REG,CURSOR_H);
	out_byte(CRTC_DATA_REG,(pos >> 8) & 0xff);
	out_byte(CRTC_ADDR_REG,CURSOR_L);
	out_byte(CRTC_DATA_REG,pos & 0xff);
	enable_int();
}

static void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}

void select_console(int nr_console) {
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES))
		return ;

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);

	u32 addr = console_table[nr_console].curr_start_addr;
	disable_int();
	out_byte(CRTC_ADDR_REG,START_ADDR_H);
	out_byte(CRTC_DATA_REG,(addr>>8)&0xff);
	out_byte(CRTC_ADDR_REG,START_ADDR_L);
	out_byte(CRTC_DATA_REG,addr&0xff);
	enable_int();
}

void scroll_screen(CONSOLE* p_con, int direction) {
	if (direction == SCR_UP && p_con->curr_start_addr > p_con->original_addr)
		p_con->curr_start_addr -= SCREEN_WIDTH;
	else if (direction == SCR_DN && p_con->curr_start_addr + SCREEN_SIZE < p_con->original_addr + p_con->v_mem_limit)
		p_con->curr_start_addr += SCREEN_WIDTH;

	set_video_start_addr(p_con->curr_start_addr);
	set_cursor(p_con->cursor);
}

void in_process(u32 key, TTY* p_tty) {
	char output[2] = {'\0','\0'};

	if (!(key & FLAG_EXT)) {
		if (p_tty->inbuf_count < TTY_IN_BYTES) {
			*(p_tty->p_inbuf_head) = key;
			p_tty->p_inbuf_head++;
			if (p_tty->p_inbuf_head == p_tty->p_inbuf + TTY_IN_BYTES)
				p_tty->p_inbuf_head = p_tty->p_inbuf;
			p_tty->inbuf_count++;
		}
	} else {
		int raw_code = key & MASK_RAW;
		switch (raw_code) {			
		case UP:
			if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
				scroll_screen(p_tty->p_console, SCR_DN);
			}
			break;
		case DOWN:
			if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R)) {
				scroll_screen(p_tty->p_console, SCR_UP);
			}
			break;
		case LEFT:
			if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R))
				select_console(nr_current_console-1);
			break;
		case RIGHT:
			if ((key & FLAG_SHIFT_L) || (key & FLAG_SHIFT_R))
				select_console(nr_current_console+1);
			break;
		case ENTER:
			if (p_tty->inbuf_count < TTY_IN_BYTES) {
				*(p_tty->p_inbuf_head) = '\n';
				p_tty->p_inbuf_head++;
				if (p_tty->p_inbuf_head == p_tty->p_inbuf + TTY_IN_BYTES)
					p_tty->p_inbuf_head = p_tty->p_inbuf;
				p_tty->inbuf_count++;
			}
			break;
		case BACKSPACE:
			if (p_tty->inbuf_count < TTY_IN_BYTES) {
				*(p_tty->p_inbuf_head) = '\b';
				p_tty->p_inbuf_head++;
				if (p_tty->p_inbuf_head == p_tty->p_inbuf + TTY_IN_BYTES)
					p_tty->p_inbuf_head = p_tty->p_inbuf;
				p_tty->inbuf_count++;
			}
			break;
		case F1:
		case F2:
		case F3:
		case F4:
		case F5:
		case F6:
		case F7:
		case F8:
		case F9:
		case F10:
		case F11:
		case F12:
			/* Alt + F1~F12 */
			break;
                default:
                        break;
                }
	}
}

void tty_write(TTY* p_tty, char* buf, int len) {
	//disp_str("write once");
	char* p = buf;
	int i = len;

	while (i) {
		//disp_str("!");
		out_char(p_tty->p_console, *p++);
		i--;
	}
}

int sys_write(char* buf, int len, PROCESS* p_proc) {
	tty_write(&tty_table[p_proc->nr_tty], buf, len);
	return 0;
}

int sys_printx(int _unused1, int _unused2, char* s, PROCESS* p_proc)
{
	const char * p;
	char ch;

	char reenter_err[] = "? k_reenter is incorrect for unknown reason";
	reenter_err[0] = MAG_CH_PANIC;

	/**
	 * @note Code in both Ring 0 and Ring 1~3 may invoke printx().
	 * If this happens in Ring 0, no linear-physical address mapping
	 * is needed.
	 *
	 * @attention The value of `k_reenter' is tricky here. When
	 *   -# printx() is called in Ring 0
	 *      - k_reenter > 0. When code in Ring 0 calls printx(),
	 *        an `interrupt re-enter' will occur (printx() generates
	 *        a software interrupt). Thus `k_reenter' will be increased
	 *        by `kernel.asm::save' and be greater than 0.
	 *   -# printx() is called in Ring 1~3
	 *      - k_reenter == 0.
	 */
	if (k_reenter == 0)  /* printx() called in Ring<1~3> */ 
		p = va2la(proc2pid(p_proc), s);
	else if (k_reenter > 0) /* printx() called in Ring<0> */
		p = s;
	else	/* this should NOT happen */
		p = reenter_err;
	/**
	 * @note if assertion fails in any TASK, the system will be halted;
	 * if it fails in a USER PROC, it'll return like any normal syscall
	 * does.
	 */
	if ((*p == MAG_CH_PANIC) ||
	    (*p == MAG_CH_ASSERT && p_proc_ready < &proc_table[NR_TASKS])) {
		disable_int();
		char * v = (char*)V_MEM_BASE;
		char * q = p + 1; /* +1: skip the magic char */
		
		while (v < (char*)(V_MEM_BASE + V_MEM_SIZE)) {
			*v++ = *q++;
			*v++ = DEFAULT_CHAR_COLOR;
			if (!*q) {
				while (((int)v - V_MEM_BASE) % (SCREEN_WIDTH * 16)) {
					// *v++ = ' ';
					v++;
					*v++ = DEFAULT_CHAR_COLOR;
				}
				q = p + 1;
			}
		}

		__asm__ __volatile__("hlt");
	}

	while ((ch = *p++) != 0) {
		if (ch == MAG_CH_PANIC || ch == MAG_CH_ASSERT)
			continue; /* skip the magic char */

		out_char(tty_table[p_proc->nr_tty].p_console, ch);
	}

	return 0;
}

#endif
