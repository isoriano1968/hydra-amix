#ifndef _AUDIO_H
#define _AUDIO_H

#define MAXCHANNELS	(4)

#ifdef _KERNEL
#define AUD_COPEN	(0x01)		/* client open */
#define AUD_CCLOSING	(0x02)		/* client in close */
#define AUD_CATTACHED	(0x04)		/* q attached to audio port */

struct audio_client
{
    int			flags;
    int			ports[2];	/* ports used */
    unsigned int	repeat;		/* time to repeat this sound */
    unsigned short	volume;
    unsigned short	period;
    queue_t		*q;
    mblk_t		*mp;		/* current block */
    struct audio_client *next;
};

struct audio_channel
{
    volatile struct amiga_audio	*audio;
    int				dmadone; /* AMIGA->intreqr; AUD block done */
    int				enable;	/* AMIGA->dmacon; AUD chan enable */
    struct audio_client		*client; /* pointer to client connected to this channel */
    int				pending; /* number of pending sounds */
    int				used;	/* buffer used */
    int				size;	/* size of buffer */
    unsigned short		nbytes[3]; /* bytes of sound copied for each buffer */
    signed char			*buffer[3]; /* data for audio */
};

#define AU_L_BESILENT	0x01		/* */
#define AU_L_NOAUDIO	0x02		/* */
#endif /* _KERNEL */

#define AUDIO_BUFFER_SIZE	8192

#define	AUIOC			('U'<<8)
#define AUDIO_VOLUME		(AUIOC|1)
#define AUDIO_PERIOD		(AUIOC|2)

#define BLOUD 63
#define BPER  400

#define	splaudio	spl7

#endif /* _AUDIO_H */
