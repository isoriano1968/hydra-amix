#ifndef _AUDIOHW_H
#define _AUDIOHW_H

#define NAUDIO 4

/*
** Description of Amiga audio hardware.
*/

struct amiga_audio
{
    signed char		*buf;		/* dma buffer */
    unsigned short	size;		/* size of buffer */
    unsigned short	period;		/* dma feed rate */
    unsigned short	loud;		/* volume */
    unsigned short	data;		/* hand feed register */
    unsigned short	pad0, pad1;
};

#endif /* _AUDIOHW_H */
