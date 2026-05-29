/*
 * acia.c : Miscellaneous routines supporting the 8520 ACIA chips.
 */

#include	"sys/types.h"
#include	"sys/conf.h"
#include	"sys/param.h"
#include	"sys/errno.h"
#include	"sys/sysmacros.h"
#include	"sys/inline.h"		/* Needed for spl*() */
#include	"sys/psw.h"
#include	"sys/pcb.h"
#include	"amigahr.h"

#ifndef CLOCK
#define CLOCK 1
#endif


/* Interrupt service routine for ACIAA (Level 2) interrupts */
void aciaaintr(pcbp)
pcb_t *pcbp;
{
	register unsigned char pending;
	pending = ACIAA->icr;		/* See who is interrupting */

	if (pending&ICR_SP)		/* Keyboard? */
		kbintr();

	if (pending&ICR_FLG)		/* Parallel port? */
		parintr();

#if CLOCK == 1
	if (pending&ICR_TA)		/* Timer? */
		if (clock_int(pcbp))
			addupc_clk(pcbp);
#endif /* CLOCK == 1 */
}


/* Interrupt service routine for ACIAB (Level 6) interrupts */
void aciabintr(pcbp)
pcb_t *pcbp;
{
	register unsigned short pending;
	pending = ACIAB->icr;		/* See who is interrupting */

#ifdef FLOPPY_INDEX
	if (pending&ICR_FLG)		/* Floppy index? */
		indexintr();
#endif /* FLOPPY_INDEX */

#if CLOCK == 2
	if (pending&ICR_TA)		/* Timer? */
		if (clock_int(pcbp))
			addupc_clk(pcbp);
#endif /* CLOCK == 2 */
}
