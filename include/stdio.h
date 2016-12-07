#ifndef	_HHX008_STDIO_H_
#define	_HHX008_STDIO_H_

# include "const.h"

/* the assert macro */
#define ASSERT
#ifdef ASSERT
void assertion_failure(char *exp, char *file, char *base_file, int line);
#define assert(exp)  if (exp) ; \
        else assertion_failure(#exp, __FILE__, __BASE_FILE__, __LINE__)
#else
#define assert(exp)
#endif

typedef	char *	va_list;
/* EXTERN */
#define	EXTERN	extern	/* EXTERN is defined as extern except in global.c */

/* string */
#define	STR_DEFAULT_LEN	1024

#define	O_CREAT		0x1
#define	O_RDWR		0x10
#define	O_TRUNC		0x100

#define SEEK_SET	1
#define SEEK_CUR	2
#define SEEK_END	3

#define	MAX_PATH	128

/**
 * @struct time
 * @brief  RTC time from CMOS.
 */

/**
 * @struct stat
 * @brief  File status, returned by syscall stat();
 */
struct stat {
	int st_dev;		/* major/minor device number */
	int st_ino;		/* i-node number */
	int st_mode;		/* file mode, protection bits, etc. */
	int st_rdev;		/* device ID (if special file) */
	int st_size;		/* file size */
};

struct time {
	u32 year;
	u32 month;
	u32 day;
	u32 hour;
	u32 minute;
	u32 second;
};

#define  BCD_TO_DEC(x)      ( (x >> 4) * 10 + (x & 0x0f) )

/*========================*
 * printf, printl, printx *
 *========================*
 *
 *   printf:
 *
 *           [send msg]                WRITE           DEV_WRITE
 *                      USER_PROC ------------→ FS -------------→ TTY
 *                              ↖______________↙↖_______________/
 *           [recv msg]             SYSCALL_RET       SYSCALL_RET
 *
 *----------------------------------------------------------------------
 *
 *   printl: variant-parameter-version printx
 *
 *          calls vsprintf, then printx (trap into kernel directly)
 *
 *----------------------------------------------------------------------
 *
 *   printx: low level print without using IPC
 *
 *                       trap directly
 *           USER_PROC -- -- -- -- -- --> KERNEL
 *
 *
 *----------------------------------------------------------------------
 */

/* printf.c */
int     printf(const char *fmt, ...);
int     printl(const char *fmt, ...);

/* vsprintf.c */
int     vsprintf(char *buf, const char *fmt, va_list args);
int	sprintf(char *buf, const char *fmt, ...);

int	open(const char *pathname, int flags);
int	close(int fd);
int	read(int fd, void *buf, int count);
int	write(int fd, const void *buf, int count);
int	unlink(const char *pathname);
int	getpid();
int	fork();
void	exit(int status);
int	wait(int * status);
int	exec(const char * path);
int	execl(const char * path, const char *arg, ...);
int	execv(const char * path, char * argv[]);
int	stat(const char *path, struct stat *buf);


#endif 

