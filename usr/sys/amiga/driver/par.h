/* Definitions for Amiga parallel port driver (/dev/par) */

/* Ioctl commands */
#define PIOC		('P'<<8)
#define PIOCGETCTL	(PIOC|1)
#define PIOCSETCTL	(PIOC|2)

/* Parallel control bits */
#define C_SEL		0x04		/* Select */
#define C_POUT		0x02		/* Paper Out */
#define C_BUSY		0x01		/* Busy */
