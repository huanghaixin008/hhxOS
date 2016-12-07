#ifndef	_HHX008_IO_H_
#define	_HHX008_IO_H_

typedef struct s_kb {
	u8* head;
	u8* tail;
	int count;
	u8 buf[KB_IN_BYTES];
}KB_INPUT;

typedef	char *	va_list;

#endif
