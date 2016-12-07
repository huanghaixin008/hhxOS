# include "const.h"
# include "protect.h"
# include "proc.h"
# include "proto.h"
# include "global.h"

void cstart() {
	disp_str_os("cstart() begins");
	memcpy(&gdt,				   /* New GDT */
	       (void*)(*((u32*)(&gdt_ptr[2]))),    /* Base  of Old GDT */
	       *((u16*)(&gdt_ptr[0])) + 1	   /* Limit of Old GDT */
	);

	u16* p_gdt_limit = (u16*)(&gdt_ptr[0]);
	u32* p_gdt_base = (u32*)(&gdt_ptr[2]);
	
	*p_gdt_limit = GDT_SIZE * sizeof(DESCRIPTOR) - 1;
	*p_gdt_base = (u32)&gdt;

	u16* p_idt_limit = (u16*)(&idt_ptr[0]);
	u32* p_idt_base = (u32*)(&idt_ptr[2]);

	*p_idt_limit = IDT_SIZE * sizeof(GATE) - 1;
	*p_idt_base = (u32)&idt;
	
	init_proc();

	disp_str_os("cstart() ends");
}
