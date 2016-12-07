# include "const.h"
# include "protect.h"
# include "proc.h"
# include "proto.h"
# include "global.h"

int fork() {
	MESSAGE msg;
	msg.type = FORK;
	
	send_recv(BOTH, TASK_MM, &msg);
	assert(msg.type == SYSCALL_RET);
	assert(msg.RETVAL == 0);

	return msg.PID;
}

int wait(int* s) {
	MESSAGE msg;
	msg.type = WAIT;
	
	send_recv(BOTH, TASK_MM, &msg);
	*s = msg.STATUS;
	assert(msg.type == SYSCALL_RET);

	return msg.PID;
}

void exit(int s) {
	MESSAGE msg;
	msg.type = EXIT;
	msg.STATUS = s;
	send_recv(BOTH, TASK_MM, &msg);
}
