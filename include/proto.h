#ifndef _HHX008_PROTO_H
#define _HHX008_PROTO_H

# include "tty.h"
# include "proc.h"
# include "io.h"
# include "fs.h"

// protocal of functions

// asm
#define	phys_copy	memcpy
#define	phys_set	memset
void memset(void* pDst, int ct, int size);
void* memcpy(void* pDst, void* pSrc, int size);
void* disp_str(char* pszInfo);
void disable_irq(int irq);
void enable_irq(int irq);
void disable_int();
void enable_int();

// protect.c
void out_byte(u16 port, u8 value);
u8 in_byte(u16 port);
void init_8259A();
void init_proc();
void init_descriptor(DESCRIPTOR* p_desc, u32 base, u32 limit, u16 attribute);
u32 seg2phys(u16 seg);
void put_irq_handler(int irq, irq_handler handler);
void spurious_irq(int irq);
void exception_handler(int vec_no, int err_code, int eip, int cs, int eflags);

// proc.c
void clock_handler(int irq);
void schedule();
int getpid();
int sys_get_ticks();
int sys_sendrec(int function, int src_dest, MESSAGE* m, PROCESS* p);
int send_recv(int function, int src_dest, MESSAGE* msg);
int ldt_seg_linear(PROCESS* p, int idx);
void* va2la(int pid, void* va);
void inform_int(int task_nr);

// io.c
void init_keyboard();
void keyboard_handler(int irq);
void keyboard_read();

// tty.c
void task_tty();
void init_tty(TTY*);
void init_screen(TTY*);
void out_char(CONSOLE*, char);
void select_console(int);
void scroll_screen(CONSOLE* p_con, int direction);
void in_process();
void tty_write(TTY* p_tty, char* buf, int len);
int sys_write(char* buf, int len, PROCESS* p_proc);
int sys_printx(int _unused1, int _unused2, char* s, PROCESS* p_proc);

// systask.c
void task_sys();

// main.c
void init_clock();
void untar(const char * filename);
void restart();
void Init();
void TestA();
void TestB();
void TestC();
void simple_shell();
int get_ticks();

// lib/klib.c
void get_boot_params(struct boot_params * pbp);
int get_kernel_map(unsigned int * b, unsigned int * l);
void strcpy(char* a, char* b);
int strlen(char* a);
char* itostr(int input, char* str);
void disp_int(int intput);
void disp_str_os(char* input);
void delay(int limit);

// lib/printf.c
int printf(const char* fmt, ...);
int printl(const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);

// lib/misc.c
int min(int a, int b);
int max(int a, int b);
int memcmp(const void * s1, const void *s2, int n);
int strcmp(const char * s1, const char *s2);
char * strcat(char * s1, const char *s2);
void spin(char * func_name);
void assertion_failure(char exp, char* file, int line);
#define assert(exp)  if (exp) ; \
        else assertion_failure('0'+exp, __FILE__, __LINE__);

// hd.c
void task_hd();
void hd_handler(int irq);

// lib/fork.c
int fork();
int wait(int* s);
void exit(int s);

// lib/exec.c
int exec(const char* path);
int execl(const char* path, const char* arg, ...);
int execv(const char* path, char* argv[]);

// fs/main.c
void task_fs();
int rw_sector(int io_type, int dev, u64 pos, int bytes, int proc_nr, void* buf);
struct super_block * get_super_block(int dev);struct inode * get_inode(int dev, int num);
void put_inode(struct inode * pinode);
void sync_inode(struct inode * p);

// fs/open_close.c
int open(const char * pathname, int flags);
int close(int fd);
int do_open();
int do_close();

// fs/read_write.c
int read(int fd, void* buf, int count);
int write(int fd, void* buf, int count);
int do_rdwt();

// fs/unlink.c
int unlink(const char * pathname);
int do_unlink();

// fs/misc.c
int stat(char* pathname, struct stat* buf);
int do_stat();
int strip_path(char * filename, const char * pathname,
		      struct inode** ppinode);
int search_file(char * path);

// mm/main.c
void task_mm();
int alloc_mem(int pid, int memsize);
int free_mem(int pid);
int do_fork();
int do_exec();
void do_wait();
void do_exit(int status);

#endif
