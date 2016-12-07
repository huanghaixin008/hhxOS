# include <const.h>
# include <protect.h>
# include <proc.h>
# include <proto.h>
# include <global.h>

void task_sys()
{
	MESSAGE msg;

	while (1) {
		send_recv(RECEIVE, ANY, &msg);
		int src = msg.source;

		switch (msg.type) {
		case GET_TICKS:
			msg.RETVAL = ticks;
			send_recv(SEND, src, &msg);
			break;
		case GET_PID:
			msg.type = SYSCALL_RET;
			msg.PID = src;
			send_recv(SEND, src, &msg);
			break;
		default:
			panic("{SYS} unknown msg type");
			break;
		}
	}
}
