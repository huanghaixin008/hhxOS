#define TTY_IN_BYTES		256	/* tty input queue size */
#define TTY_OUT_BUF_LEN		32	/* tty output buffer size */

struct s_console;


/* TTY */
typedef struct s_tty
{
	u32 	p_inbuf[TTY_IN_BYTES];
	u32* 	p_inbuf_head;
	u32* 	p_inbuf_tail;
	int	inbuf_count;

	int 	tty_caller;
	int 	tty_procnr;
	void*	tty_req_buf;
	int	tty_left_cnt;
	int	tty_trans_cnt;

	struct	s_console*	p_console;
}TTY;

typedef struct s_console
{
	unsigned int	curr_start_addr;
	unsigned int	original_addr;
	unsigned int 	v_mem_limit;
	unsigned int 	cursor;
}CONSOLE;
