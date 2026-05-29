#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/inline.h>
#include "memory.h"
#include "amigahr.h"
#include "audio.h"

extern char *panicstr;

/* STREAM interface functions */
static int audioopen(queue_t *, dev_t *, int, int, cred_t *);
static int audioclose(queue_t *);
static int audiowput(struct queue *, mblk_t *);
static int audiowsrv(struct queue *);

/* STREAM support functions */
static void audioioctl(queue_t *, mblk_t *);

static struct module_info audiomiinfo =
{
    0, "audio", 0, INFPSZ, AUDIO_BUFFER_SIZE, 128,
};

static struct qinit audiorinit =
{
    NULL, NULL, audioopen, audioclose, NULL, &audiomiinfo,
};

static struct module_info audiomoinfo =
{
    42, "audio", 0, INFPSZ, 300, 200,
};

static struct qinit audiowinit =
{
    audiowput, audiowsrv, audioopen, audioclose, NULL, &audiomoinfo,
};

struct streamtab audioinfo =
{
    &audiorinit, &audiowinit, NULL, NULL,
};

static int postaudio(queue_t *q, mblk_t *);
static void handfeed(struct audio_client *);
static int grab_channel(struct audio_client *);
static int fill_buffer(struct audio_client *, signed char *, int);
static int allocate_channel_buffers(struct audio_client *);
static void loadup_dma(struct audio_client *, int);
static void release_channel_buffer(struct audio_channel *);
void audiointr(unsigned int);

#define blklen(bp)	(bp->b_wptr - bp->b_rptr)

static signed char audio_silent_buffer[2];

/*
** audio_channel: one for each audio port we have.  The standard
** amiga has four channels, two dedicated to the left speaker, and
** two to the right speaker.
*/
static struct audio_channel audio_channel[MAXCHANNELS] =
{
    { NULL, AIEAUD0, DMAAUD0, },
    { NULL, AIEAUD1, DMAAUD1, },
    { NULL, AIEAUD2, DMAAUD2, },
    { NULL, AIEAUD3, DMAAUD3, },
};

static struct audio_client audio_clients;

void
audioinit()
{
    int i;
    static boolean_t initialized;

    if (initialized)
	return;				/* already initialized */

    for (i=0; i < MAXCHANNELS; ++i)
    {
	AMIGA->intena = AINTCLR | audio_channel[i].dmadone; /* disable interupt */
	AMIGA->intreq = AINTCLR | audio_channel[i].dmadone; /* clear interupt */
	AMIGA->dmacon = DMACLR | audio_channel[i].enable; /* disable DMA */
	audio_channel[i].audio = &AMIGA->audio[i];
	audio_channel[i].audio->buf = audio_silent_buffer;
	audio_channel[i].audio->size = 1;
	audio_channel[i].audio->period = 1;
	audio_channel[i].audio->loud = 0;
	audio_channel[i].audio->data = 0;
	/* <IMPLEMENT> There should be a timeout here incase */
	/* <IMPLEMENT> the audio hardware is broken */
	while (!(AMIGA->intreqr&audio_channel[i].dmadone))
	    ;			/* busy wait for hand fed word */
	AMIGA->intreq = AINTCLR | audio_channel[i].dmadone; /* clear interupt */
    }

    initialized = B_TRUE;
}

static int
audioopen(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
    struct audio_client *acl;
    int s;

    if (sflag == MODOPEN)
	return EINVAL;

    if ((acl = kmem_alloc(sizeof(*acl), 0)) == NULL)
	return ENOMEM;

    s = splaudio();

    audioinit();

    acl->flags = AUD_COPEN;
    acl->ports[0] = acl->ports[1] = 0;
    acl->volume = BLOUD;
    acl->period = BPER;
    acl->repeat = 0;
    acl->q = WR(q);
    acl->mp = NULL;
    acl->next = audio_clients.next;
    audio_clients.next = acl;
    splx(s);

    q->q_ptr = acl;
    WR(q)->q_ptr = acl;

    return 0;
}

static int
audioclose(queue_t *q)
{
    struct audio_client *acl = (struct audio_client *)q->q_ptr, *aclp;
    int s;

    s = splaudio();

#if 0
    while (WR(q)->q_first)
    {
	/* There are some requests pending */

	acl->flags |= AUD_CCLOSING;

	if (sleep(acl, PZERO+1|PCATCH))
	{
	    /* Flush all audios with this client id.  We're outta here */
	    acl->flags &= ~AUD_CCLOSING;
#if 0
	    flushaudio(acl);
#endif
	    break;
	}
    }
#endif

    /* Remove us from the clients list */

    for (aclp = &audio_clients; aclp->next != acl; aclp = aclp->next)
	;

    aclp->next = aclp->next->next;

    splx(s);

    kmem_free(acl, sizeof(*acl));

    q->q_ptr = NULL;
    WR(q)->q_ptr = NULL;

    return 0;
}

static int
audiowput(struct queue *q, mblk_t *mp)
{
    switch (mp->b_datap->db_type)
    {
    case M_DATA:
	{
	    int s = splaudio();
	    putq(q, mp);
	    splx(s);
	    break;
	}

    case M_IOCTL:
    case M_IOCDATA:
	audioioctl(q, mp);
	break;

    default:
	freemsg(mp);
	break;
    }

    return 0;
}

static int
audiowsrv(queue_t *q)
{
    mblk_t *mp;
    struct audio_client *acl = (struct audio_client *)q->q_ptr;
    int s;

    s = splaudio();
    if (acl->mp)
    {
	if (postaudio(q, acl->mp))
	{
	    splx(s);
	    return 1;
	}

	freemsg(acl->mp);
	acl->mp = NULL;
    }

    while ((mp = getq(q, mp)) != NULL)
    {
	if (postaudio(q, mp))
	{
	    acl->mp = mp;
	    splx(s);
	    return 1;
	}

	freemsg(mp);
    }

    splx(s);
    return 0;
}

static void
audioioctl(q, mp)
queue_t *q;
mblk_t *mp;
{
    register struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;
    register struct audio_client *audio_client =
	(struct audio_client *)q->q_ptr;
    
    if (mp->b_datap->db_type == M_IOCDATA)
    {
	/* For copyin/copyout failures, just free message. */
	if (((struct copyresp *)mp->b_rptr)->cp_rval)
	{
	    freemsg(mp);
	    return;
	}

	if (!((struct copyresp *)mp->b_rptr)->cp_private)
	{
	    mp->b_datap->db_type = M_IOCACK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = 0;
	    putnext(RD(q), mp);
	    return;
	}
    }

    switch (iocbp->ioc_cmd)
    {
    case AUDIO_VOLUME:
	{
	    int *volume;

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
	    {
		struct copyreq *creq = (struct copyreq *)mp->b_rptr;
		mp->b_datap->db_type = M_COPYIN;
		creq->cq_addr = *(caddr_t *)mp->b_cont->b_rptr;
		mp->b_wptr = mp->b_rptr + sizeof(*creq);
		creq->cq_size = sizeof(*volume);
		creq->cq_flag = 0;
		creq->cq_private = (mblk_t *)1;
		putnext(RD(q), mp);
		return;
	    }
	    else
	    {
		mblk_t *bp1;
		register int i;

		(void)pullupmsg(mp->b_cont, -1);

		if (!mp->b_cont || blklen(mp->b_cont) != sizeof(*volume))
		{
		    iocbp->ioc_error = EINVAL;
		    mp->b_datap->db_type = M_IOCNAK;
		    iocbp->ioc_count = 0;
		    putnext(RD(q), mp);
		    break;
		}

		volume = (int *)mp->b_cont->b_rptr;

		mp->b_datap->db_type = M_IOCACK;

		audio_client->volume = *volume;

		bp1 = unlinkb(mp);
		if (bp1)
		    freeb(bp1);
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);

	    }
	    break;
	}

    case AUDIO_PERIOD:
	{
	    int *period;

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
	    {
		struct copyreq *creq = (struct copyreq *)mp->b_rptr;
		mp->b_datap->db_type = M_COPYIN;
		creq->cq_addr = *(caddr_t *)mp->b_cont->b_rptr;
		mp->b_wptr = mp->b_rptr + sizeof(*creq);
		creq->cq_size = sizeof(*period);
		creq->cq_flag = 0;
		creq->cq_private = (mblk_t *)1;
		putnext(RD(q), mp);
		return;
	    }
	    else
	    {
		mblk_t *bp1;
		register int i;

		(void)pullupmsg(mp->b_cont, -1);

		if (!mp->b_cont || blklen(mp->b_cont) != sizeof(*period))
		{
		    iocbp->ioc_error = EINVAL;
		    mp->b_datap->db_type = M_IOCNAK;
		    iocbp->ioc_count = 0;
		    putnext(RD(q), mp);
		    break;
		}

		period = (int *)mp->b_cont->b_rptr;

		mp->b_datap->db_type = M_IOCACK;

		audio_client->period = *period;

		bp1 = unlinkb(mp);
		if (bp1)
		    freeb(bp1);
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);

	    }
	    break;
	}

    default:
	mp->b_datap->db_type = M_IOCNAK;
	freemsg(unlinkb(mp));
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	iocbp->ioc_error = EINVAL;
	putnext(RD(q), mp);
	return;
    }

    mp->b_datap->db_type = M_IOCACK;
    freemsg(unlinkb(mp));
    iocbp->ioc_count = 0;
    iocbp->ioc_rval = 0;
    iocbp->ioc_error = 0;
    putnext(RD(q), mp);
    return;
}

void
audiointr(unsigned int reqbits)
{
    int i;
    int s;

    s = splaudio();

    if (panicstr)			/* System panic? */
    {
	/* Turn off audio ports */
	AMIGA->intena = AINTCLR | (AIEAUD0|AIEAUD1|AIEAUD2|AIEAUD3); /* interupts */
	AMIGA->dmacon = DMACLR | (DMAAUD0|DMAAUD1|DMAAUD2|DMAAUD3); /* DMA */
	splx(s);
	return;
    }

    for (i=0; i < MAXCHANNELS; ++i)
    {
	/*
	** This channel is in use.  Check to see if the DMA finished
	** our last request and post another audio if it has.  If this
	** queue is empty, remove it from the audio_channel queue,
	** disable the audio channel and wake up the client if it
	** is sleeping in close.
	*/

	if (reqbits & (1 << i))
	{
	    struct audio_client *acl = audio_channel[i].client;
	    /* DMA is finished for this request */

	    if (acl && acl->repeat)
	    {
		--acl->repeat;
		continue;
	    }

	    release_channel_buffer(&audio_channel[i]);

	    if (acl != NULL && acl->q != NULL)
	    {
#if 0
		qenable(acl->q);
#else
		mblk_t *mp;
		int size, blen;

		if (acl->mp)
		{
		    blen = blklen(acl->mp);
		    size = fill_buffer(acl, (signed char *)acl->mp->b_rptr, blen);

		    if (size == blen)
		    {
			freemsg(acl->mp);
			acl->mp = NULL;
		    }
		}

		if (!acl->mp)
		{
		    while ((mp = getq(acl->q, mp)) != NULL)
		    {
			blen = blklen(mp);
			size = fill_buffer(acl, (signed char *)mp->b_rptr, blen);

			if (size != blen)
			{
			    acl->mp = mp;
			    break;
			}

			freemsg(mp);
		    }
		}
#endif
	    }

	    if (audio_channel[i].nbytes[audio_channel[i].used])
	    {
		/* The next buffer for this channel has data in it.  load it up and go */
		loadup_dma(acl, 0);
	    }
	    else
	    {
		/*
		** No more audios ready.  Give up this port.
		*/

		if (acl)	
		{
		    fill_buffer(acl, audio_silent_buffer, 2);
		    loadup_dma(acl, 0);

		    audio_channel[i].client = NULL;

		    acl->flags &= ~AUD_CATTACHED;
		    if (acl->flags&AUD_CCLOSING)
			wakeup(acl);
		} else if (audio_channel[i].pending == 0)
		{
		    AMIGA->intena = AINTCLR | audio_channel[i].dmadone;
		    AMIGA->dmacon = DMACLR | audio_channel[i].enable;
		}
#if 1
		else if (audio_channel[i].pending < 0)
		{
		    printf("WARNING: Pending[%d] reached %d\n", i, audio_channel[i].pending);
		    audio_channel[i].pending = 0;
		    AMIGA->intena = AINTCLR | audio_channel[i].dmadone;
		    AMIGA->dmacon = DMACLR | audio_channel[i].enable;
		}
#endif
	    }
	}
    }

    /* Search client list for any audios to send out */

    {
	struct audio_client *acl;

	for (acl = &audio_clients; acl->next; acl = acl->next)
	{
	    if (acl->next->q && !(acl->next->flags&AUD_CATTACHED) && acl->next->mp != NULL)
	    {
		if (postaudio(acl->next->q, acl->next->mp))
		{
		    acl->mp = acl->next->mp;
		}
		else
		{
		    freemsg(acl->next->mp);
		    acl->next->mp = NULL;
		}

		qenable(acl->next->q);
	    }
	}
    }

#undef BUGGYBUTTONS
#ifdef BUGGYBUTTONS
    if (!(AMIGA->potinp & 1<<8))
	while ((AMIGA->potinp & 1<<10))
	    ;
#endif /* BUGGYBUTTONS */

    splx(s);
}

int
postaudio(queue_t *q, mblk_t *mp)
{
    struct audio_client *acl = (struct audio_client *)q->q_ptr;
    struct audio_channel *ach[2];
    int size;

    if (grab_channel(acl))
	return 1;			/* <XXX> Could return actual error code */

    ach[0] = &audio_channel[acl->ports[0]];

    ach[0]->client = acl;

    switch (ach[0]->pending)
    {
    case 0:				/* No sound currently playing. */
	/*
	** Handfeed first byte and load up the audio hardware to grab
	** dma registers at next scan line.  This is the only way to
	** garrantee that we will play the first dma'd audio sound to
	** a channel.
	*/

	size = fill_buffer(acl, (signed char *)mp->b_rptr, blklen(mp));
	mp->b_rptr += size;
	handfeed(acl);
	break;

    case 1:				/* One sound is playing. */
	/*
	** Set the dma registers to play this audio next time around.
	*/
	size = fill_buffer(acl, (signed char *)mp->b_rptr, blklen(mp));
	mp->b_rptr += size;
	loadup_dma(acl, 0);
	break;

    default:				/* Both buffers are full. */
	/*
	** The next filled buffer will be loaded at interupt time when the
	** currently playing buffer has finished.
	*/
	break;
    }

    return  !(mp->b_rptr == mp->b_wptr);
}

static int
fill_buffer(struct audio_client *acl, signed char *data, int len)
{
    int size;
    struct audio_channel *ach[2];

    ach[0] = &audio_channel[acl->ports[0]];
    
    size = min(len, ach[0]->size - ach[0]->nbytes[ach[0]->used]);

    bcopy(data, ach[0]->buffer[ach[0]->used] + ach[0]->nbytes[ach[0]->used], size);
    ach[0]->nbytes[ach[0]->used] += size;

    return size;
}

static int
grab_channel(struct audio_client *acl)
{
    int i;

    if (!(acl->flags&AUD_CATTACHED))	/* This client has no dedicated port */
    {
	/* First check if old port is available */

	if (audio_channel[acl->ports[0]].client != NULL)
	{
	    for (i=0; i < MAXCHANNELS; ++i)
	    {
		if (audio_channel[i].client == NULL) /* is channel allocated? */
		{
		    acl->ports[0] = i;	/* no, this is the port we will use */
		    break;
		}
	    }

	    if (i >= MAXCHANNELS)
	    {
		return EBUSY;		/* not enough channels free */
	    }
	}

	acl->flags |= AUD_CATTACHED;
    }

    if (allocate_channel_buffers(acl))
    {
	acl->flags &= ~AUD_CATTACHED;
	return ENOMEM;
    }

    return 0;
}

static int
allocate_channel_buffers(struct audio_client *acl)
{
    struct audio_channel *ach[2];

    ach[0] = &audio_channel[acl->ports[0]];

    if (ach[0]->size == 0)
    {
	if (!(ach[0]->buffer[0] = AllocMem(AUDIO_BUFFER_SIZE, MEMF_CHIP)))
	    return ENOMEM;
	if (!(ach[0]->buffer[1] = AllocMem(AUDIO_BUFFER_SIZE, MEMF_CHIP)))
	{
	    /* free ach[0]->buffer[0] before returning */
	    FreeMem(ach[0]->buffer[0], AUDIO_BUFFER_SIZE);
	    return ENOMEM;
	}
	if (!(ach[0]->buffer[2] = AllocMem(AUDIO_BUFFER_SIZE, MEMF_CHIP)))
	{
	    /* free ach[0]->buffer[0...1] before returning */
	    FreeMem(ach[0]->buffer[0], AUDIO_BUFFER_SIZE);
	    FreeMem(ach[0]->buffer[1], AUDIO_BUFFER_SIZE);
	    return ENOMEM;
	}

	ach[0]->size = AUDIO_BUFFER_SIZE;
	ach[0]->nbytes[0] = ach[0]->nbytes[1] = ach[0]->nbytes[2] = 0;
	ach[0]->pending = 0;
    }

    return 0;
}

static void
handfeed(struct audio_client *acl)
{
    struct audio_channel *ach[2];

    ach[0] = &audio_channel[acl->ports[0]];

    AMIGA->intena = AINTCLR | ach[0]->dmadone;	/* disable interupts */
    AMIGA->dmacon = DMACLR | ach[0]->enable; /* disable dma for now */
    AMIGA->intreq = AINTCLR | ach[0]->dmadone;	/* clear interupt */

    loadup_dma(acl, AU_L_NOAUDIO);
    ach[0]->audio->data = 0;		/* We feed the audio hardware a silent yell */
					/* until we can post the actual sound in */
					/* the interupt routine */

    while (!(AMIGA->intreqr&ach[0]->dmadone))
	;				/* busy wait for hand fed word */

    AMIGA->dmacon = DMASET | ach[0]->enable; /* enable dma */
    AMIGA->intreq = AINTCLR | ach[0]->dmadone;	/* clear interupt */
    AMIGA->intena = AINTSET | ach[0]->dmadone;	/* enable interupts */

    ach[0]->audio->loud = acl->volume;
    ach[0]->audio->period = acl->period;
}

static void
loadup_dma(struct audio_client *acl, int flags)
{
    struct audio_channel *ach[2];

    ach[0] = &audio_channel[acl->ports[0]];

    if (ach[0]->nbytes[ach[0]->used] == 0)
    {
	printf("WARNING loadup_dma called with no buffer ready\n");
	return;
    }

    ach[0]->audio->buf = ach[0]->buffer[ach[0]->used];
    ach[0]->audio->size = ach[0]->nbytes[ach[0]->used] / 2;

    if (ach[0]->used == 2)
	ach[0]->used = 0;
    else
	++ach[0]->used;

    if (flags&AU_L_NOAUDIO)
    {
	ach[0]->audio->period = 1;
	ach[0]->audio->loud = 0;
    }
    else
    {
	ach[0]->audio->period = acl->period;
	ach[0]->audio->loud = acl->volume;
    }

    ++ach[0]->pending;
}

static void
release_channel_buffer(struct audio_channel *ach)
{
    if (ach->pending > 0)
    {
	int old = ach->used;

	if (old == 0)
	    old = 2;
	else
	    --old;

	--ach->pending;

	if (ach->pending > 0)
	{
	    if (old == 0)
		old = 2;
	    else
		--old;
	}

	ach->nbytes[old] = 0;
    }
#ifdef 1				/* Just an Assert */
    else /* if (ach->pending <= 0) */
    {
	printf("WARNING; audiodriver: channel 0x%x, pending %d\n", ach, ach->pending);
	ach->pending = 0;
	return;
    }
#endif
}
