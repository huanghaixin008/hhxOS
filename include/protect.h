#ifndef _HHX008_PROTECT_H
#define _HHX008_PROTECT_H

#define vir2phys(seg_base, vir) (u32)(((u32)seg_base) + (u32)(vir))
/* seg:off -> linear addr */
#define makelinear(seg,off) (u32)(((u32)(seg2phys(seg))) + (u32)(off))

#define	reassembly(high, high_shift, mid, mid_shift, low)	\
	(((high) << (high_shift)) +				\
	 ((mid)  << (mid_shift)) +				\
	 (low))

typedef void (*int_handler) ();
typedef void (*irq_handler) (int irq);
typedef void* system_call;

typedef struct seg_descriptor {
	u16 limit_low;
	u16 base_low;
	u8 base_mid;
	u8 attr1;
	u8 limit_high_attr2;
	u8 base_high;
} DESCRIPTOR;

typedef struct s_gate {
	u16 offset_low;
	u16 selector;
	u8 dcount;
	u8 attr;
	u16 offset_high;
} GATE;

typedef struct s_tss {
	u32	backlink;
	u32	esp0;	/* stack pointer to use during interrupt */
	u32	ss0;	/*   "   segment  "  "    "        "     */
	u32	esp1;
	u32	ss1;
	u32	esp2;
	u32	ss2;
	u32	cr3;
	u32	eip;
	u32	flags;
	u32	eax;
	u32	ecx;
	u32	edx;
	u32	ebx;
	u32	esp;
	u32	ebp;
	u32	esi;
	u32	edi;
	u32	es;
	u32	cs;
	u32	ss;
	u32	ds;
	u32	fs;
	u32	gs;
	u32	ldt;
	u16	trap;
	u16	iobase;	/* I/O位图基址大于或等于TSS段界限，就表示没有I/O许可位图 */
} TSS;

#endif
