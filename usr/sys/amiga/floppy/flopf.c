/*
 * This file contains the routines which do track I/O on floppies
 * formatted for Amiga DOS.
 */
#include "hstyle.h"
#include "flopf.h"


#define	BPL	32			/* bits per ulong */
#define	RETRY	5
#define	DMASK	0x55555555		/* mask over data bits (MFM) */


/*
 * The size macro gives the size of a member of a typedef struct.
 */
#define	size(type, mem)		(sizeof(((type *)NULL)->mem))


/*
 * Functions that drive the hardware.
 */
extern void	flinit(),		/* initialize hardware */
		flstop(),		/* turn motor off */
		fllost();		/* issue recal next operation */
extern bool	flread(),		/* read a track */
		flwrite();		/* write a track */


/*
 * Externally visible functions.
 * Note, the difference between puttrk() and puttrkus() is that the
 * former makes sure that the drive is on track.  This requires reading
 * part of the track.
 */
extern void	fdamtbuf();		/* set chip memory track buffer */
extern bool	fdgetaml(),		/* read Amiga low density track */
		fdputaml();		/* write Amiga low density track */


/*
 * Local functions.
 */
static bool	get(),			/* try to read track */
		gethdr(),		/* decode and check sec. hdr. */
		chktrk();		/* check current track */
static void	putslop(),		/* add track slop */
		putsec(),		/* encode sector */
		putmagic();		/* add MFM sync key */
static bool	find();			/* find sector start */
static void	demfm(),		/* remove MFM clock bits */
		mfm();			/* add MFM clock bits */
static ulong	chksum();		/* checksum a block */


/*
 * Local variables.
 */
static char	*trkbuf;		/* chip memory track buffer */
static uint	drive,			/* drive we are working on */
		track;			/* track we are working on */
static char	*buffer,		/* user buffer */
		*mfmbuf,		/* mfm buffer */
		*mfmlim;		/* endof mfmbuf */
static uint	shift;			/* bit shift */
static sechdr	hdr;			/* current sector header */


/*
 * Set chip memory track buffer.
 * Note, the buffer must be atleast TRKBLEN long.
 */
void
fdamtbuf(bp, size)
char	*bp;
int	size;
{
	if (size < TRKBLEN) {
		printf("Floppy chip buffer too small (%d < %d)\n",
			size, TRKBLEN);
		panic("");
	}
	trkbuf = bp;
}


/*
 * Read in a track.  This includes (bit) aligning the track, MFM
 * decoding and check-summing.
 * Note, buff must point to an area which is NSEC * BPSEC bytes long.
 */
bool
fdgetaml(drv, trk, buff)
uint	drv,
	trk;
char	*buff;
{
	register uint	n;

	drive = drv;
	track = trk;
	buffer = buff;
	for (n = RETRY; not get(); --n)
		if (n == 0) {
			printf("Floppy: unrecoverable read error\n");
			return (FALSE);
		} else {
			fllost(drive);
			printf("Floppy: read error (retrying)\n");
		}
	return (TRUE);
}


static bool
get()
{
	register uint	sec,
			nsec,
			n;
	register char	*p;

	if (not flread(drive, track, trkbuf, TRKRLEN))
		return (FALSE);		/* read failure */
	mfmbuf = trkbuf;
	mfmlim = trkbuf + TRKRLEN;
	if (not find())
		return (FALSE);		/* can't find any sectors */
	nsec = 0;
	if (not gethdr())
		return (FALSE);		/* bad header */
	sec = hdr.sec;
	n = hdr.ord;
	p = buffer + sec*BPSEC;
	loop {
		demfm((ulong *)p, size(mfmsec, data));
		if (chksum((ulong *)p, BPSEC) != hdr.datachk)
			return (FALSE);	/* data check sum error */
		if (++nsec == NSEC)
			return (TRUE);	/* hard to believe */
		p += BPSEC;
		if (++sec == NSEC) {
			sec = 0;
			p = buffer;
		}
		if (--n == 0) {
			n = NSEC;
			if (not find())
				return (FALSE);	/* can't find rest */
		}
		if ((not gethdr())
		or  (hdr.sec != sec)
		or  (hdr.ord != n))
			return (FALSE);	/* bad header */
	}
}


/*
 * Gethdr gets a sector header and checks it for validity.
 */
static bool
gethdr()
{
	register ulong	m;

	if (mfmbuf + sizeof(mfmsec) + sizeof(ushort) > mfmlim)
		return (FALSE);		/* not enough bytes */
	m = ((ulong *)mfmbuf)[0] << shift
		| ((ulong *)mfmbuf)[1] >> BPL-shift;
	m &= ~ (1 << BPL-1);
	if (m != 0x2AAAAAAA)
		return (FALSE);		/* bad MFM magic */
	m = ((ulong *)mfmbuf)[1] << shift
		| ((ulong *)mfmbuf)[2] >> BPL-shift;
	if (m != 0x44894489)
		return (FALSE);		/* bad MFM magic */
	mfmbuf += size(mfmsec, magic);
	demfm((ulong *)&hdr.magic, size(mfmsec, id));
	demfm((ulong *)hdr.label, size(mfmsec, label));
	demfm((ulong *)&hdr.hdrchk, size(mfmsec, hdrchk));
	demfm((ulong *)&hdr.datachk, size(mfmsec, datachk));
	if ((hdr.magic != SECOK)
	or  (hdr.trk >= TRACKS)
	or  (hdr.sec >= NSEC)
	or  (hdr.ord-1 >= NSEC))
		return (FALSE);		/* crazy values */
	m = chksum((ulong *)&hdr, (char *)&hdr.hdrchk - (char *)&hdr);
	if (hdr.hdrchk != m)
		return (FALSE);		/* bad checksum */
	if (hdr.trk != track)
		return (FALSE);
	return (TRUE);
}


/*
 * Write out a track.  This includes MFM encoding and check-summing.
 * Note, buff must point to an area which is NSEC * BPSEC bytes long.
 * If `check' is TRUE, then we first check to make sure that we are
 * on track.
 */
bool
fdputaml(drv, trk, buff, check)
uint	drv,
	trk;
char	*buff;
bool	check;
{
	register uint	n;

	drive = drv;
	track = trk;
	if (check) {
		for (n = RETRY; not chktrk(); --n)
			if (n == 0) {
				printf("Floppy: unrecoverable seek error\n");
				return (FALSE);
			} else
				printf("Floppy: seek error (retrying)\n");
	}
	buffer = buff;
	mfmbuf = trkbuf;
	mfmlim = trkbuf + TRKWLEN;
	putslop();
	for (n = 0; n != NSEC; ++n)
		putsec(n);
	/* just one more word of slop */
	*(ushort *)mfmbuf = (mfmbuf[-1]&1) ? 0x2AA8 : 0xAAA8;
	mfmbuf += sizeof(ushort);
	assert(mfmbuf == mfmlim);
	for (n = RETRY; not flwrite(drive, track, trkbuf, TRKWLEN); --n)
		if (n == 0) {
			printf("Floppy: unrecoverable write error\n");
			return (FALSE);
		} else
			printf("Floppy: write error (retrying)\n");
	return (TRUE);
}


/*
 * Chktrk checks to make sure that we are on track before writing.
 */
static bool
chktrk()
{
	if (not flread(drive, track, trkbuf, CHKLEN))
		return (FALSE);
	mfmbuf = trkbuf;
	mfmlim = trkbuf + CHKLEN;
	if ((find())
	and (gethdr())
	and (hdr.trk == track))
		return (TRUE);
	fllost(drive);
	return (FALSE);
}


/*
 * Putslop puts TRKSLOP bytes worth of MFM'd 0's onto mfmbuf, leaving
 * it advanced.  Note, some of these bits will be overwritten by the
 * end of the last sector, but we want to be sure that any that
 * survive don't contain the magic MFM synchronization key.
 */
static void
putslop()
{
	register ulong	*p;
	register uint	n;

	assert(TRKSLOP % sizeof(ulong) == 0);
	assert(TRKSLOP != 0);
	assert(mfmbuf + TRKSLOP <= mfmlim);
	p = (ulong *)mfmbuf;
	n = TRKSLOP / sizeof(ulong);
	do {
		*p++ = 0xAAAAAAAA;	/* MFM 0's */
	} while (--n != 0);
	mfmbuf = (char *)p;
}


/*
 * Putsec encodes the next sector (number sec) from buffer to mfmbuf,
 * advancing both.  Note, it assumes that the sectors are being written
 * from 0 to NSEC-1, so that the `ord' member is always NSEC - sec.
 */
static void
putsec(sec)
uint	sec;
{
	register uchar	*cp;

	assert(mfmbuf + sizeof(mfmsec) <= mfmlim);
	putmagic();
	hdr.magic = SECOK;
	hdr.trk = track;
	hdr.sec = sec;
	hdr.ord = NSEC - sec;
	for (cp = hdr.label; cp != endof(hdr.label); ++cp)
		*cp = 0;
	hdr.hdrchk = chksum((ulong *)&hdr, (char *)&hdr.hdrchk-(char *)&hdr);
	hdr.datachk = chksum((ulong *)buffer, BPSEC);
	mfm((ulong *)&hdr.magic, size(mfmsec, id));
	mfm((ulong *)hdr.label, size(mfmsec, label));
	mfm((ulong *)&hdr.hdrchk, size(mfmsec, hdrchk));
	mfm((ulong *)&hdr.datachk, size(mfmsec, datachk));
	mfm((ulong *)buffer, size(mfmsec, data));
	buffer += BPSEC;
}


/*
 * Putmagic adds the magic MFM synchronizing key onto mfmbuf, leaving
 * it advanced.
 */
static void
putmagic()
{
	register mfmsec	*p;

	p = (mfmsec *)mfmbuf;
	p->magic[0] = (p->magic[-1]&1) ? 0x2AAA : 0xAAAA;
	p->magic[1] = 0xAAAA;
	p->magic[2] = 0x4489;
	p->magic[3] = 0x4489;
	mfmbuf += sizeof(p->magic);
}


/*
 * Find the start of a sector.
 * Basically, this involves searching for the bit string
 *	AAAA AAAA 4489 4489
 * Actually, following Amiga DOS, we simply require a string of
 * 0xAAAA's or 0x5555's word aligned followed by 4489 4489.
 * It advances `mfmbuf' to the first word of the sector (including
 * the above bit string) and sets `shift' to the number of bits to
 * throw out of this word.
 */
static bool
find()
{
	register ushort		*fwa;
	register struct bittab	*tp;
	register ushort		w;
	int			n;
	static struct	bittab {
		ushort	word0,
			word1,
			word2,
			w2mask,
			shift;
	}	even[]	= {
			{ 0x4489, 0x4489, 0x0000, 0x0000, 0 },
			{ 0xA448, 0x9448, 0x9000, 0xF000, 4 },
			{ 0xAA44, 0x8944, 0x8900, 0xFF00, 8 },
			{ 0xAAA4, 0x4894, 0x4890, 0xFFF0, 12 },
			{ 0x9122, 0x5122, 0x4000, 0xC000, 2 },
			{ 0xA912, 0x2512, 0x2400, 0xFC00, 6 },
			{ 0xAA91, 0x2251, 0x2240, 0xFFC0, 10 },
			{ 0xAAA9, 0x1225, 0x1224, 0xFFFC, 14 }
		 },
		odd[]	= {
			{ 0x4891, 0x2891, 0x2000, 0xE000, 3 },
			{ 0x5489, 0x1289, 0x1200, 0xFE00, 7 },
			{ 0x5548, 0x9128, 0x9120, 0xFFE0, 11 },
			{ 0x5554, 0x8912, 0x8912, 0xFFFE, 15 },
			{ 0x2244, 0xA244, 0x8000, 0x8000, 1 },
			{ 0x5224, 0x4A24, 0x4800, 0xF800, 5 },
			{ 0x5522, 0x44A2, 0x4480, 0xFF80, 9 },
			{ 0x5552, 0x244A, 0x2448, 0xFFF8, 13 }
		};

	assert(mfmbuf <= mfmlim);
	assert((uint)mfmbuf%sizeof(ushort) == (uint)mfmlim%sizeof(ushort));
	fwa = (ushort *)mfmbuf;
	if (fwa == (ushort *)mfmlim)
		return (FALSE);
	++fwa;
	while (fwa != (ushort *)mfmlim) {
		w = *fwa++;
		switch (w) {
		case 0xAAAA:
			tp = even;
			break;
		case 0x5555:
			tp = odd;
			break;
		default:
			continue;
		}
		loop {
			if (fwa + 2 >= (ushort *)mfmlim)
				return (FALSE);
			if (*fwa != w)
				break;
			++fwa;
		}
		for (n = cardof(even); n != 0; --n, ++tp)
			if ((fwa[0] == tp->word0)
			and (fwa[1] == tp->word1)
			and ((fwa[2] & tp->w2mask) == tp->word2)) {
				shift = tp->shift;
				mfmbuf = (char *)(fwa - 2);
				return (TRUE);
			}
	}
	return (FALSE);
}


/*
 * Demfm decodes MFM data from mfmbuf (which it leaves advanced).
 * Note, `len' is the length, in bytes, of the encoded data.
 */
static void
demfm(to, len)
register ulong	*to;
uint		len;
{
	register ulong	*odd,
			*even;

	assert(len % (2*sizeof(ulong)) == 0);
	assert(len != 0);
	odd = (ulong *)mfmbuf;
	mfmbuf += len;
	assert(mfmbuf <= mfmlim);
	len /= 2 * sizeof(ulong);
	even = odd + len;
	do {
		*to++ = ((odd[0] << shift | odd[1] >> BPL-shift) & DMASK) << 1
			| (even[0] << shift | even[1] >> BPL-shift) & DMASK;
		++odd;
		++even;
	} while (--len != 0);
}


/*
 * Mfm encodes raw data into mfmbuf (which it leaves advanced).
 * The rule for MFM is that a clock bit is set iff the two
 * adjacent data bits are both 0.  Note, clock bits are in the
 * odd bits and data bits are in the odd bits.
 * Note, `len' is the length, in bytes, of the encoded data.
 * Note, we assume that something is already in the buffer (since
 * we look at the last bit to adjust the first MFM clock bit).
 */
static void
mfm(from, len)
register ulong	*from;
uint		len;
{
	register ulong	n,
			*odd,
			*even;

	assert(len % (2*sizeof(ulong)) == 0);
	assert(len != 0);
	assert(mfmbuf + len <= mfmlim);
	odd = (ulong *)mfmbuf;
	mfmbuf += len;
	assert(mfmbuf <= mfmlim);
	len /= 2 * sizeof(ulong);
	even = odd + len;
	do {
		n = *from>>1 & DMASK;
		n |= ~ (n<<1 | n>>1 | DMASK);	/* aren't I clever */
		if (((n & 1<<BPL-1) != 0)
		and ((odd[-1] & 1<<0) != 0))
			n &= ~ (1<<BPL-1);
		*odd++ = n;
		n = *from++ & DMASK;
		n |= ~ (n<<1 | n>>1 | DMASK);
		if (((n & 1<<BPL-1) != 0)
		and ((even[-1] & 1<<0) != 0))
			n &= ~ (1<<BPL-1);
		*even++ = n;
	} while (--len != 0);
	n = *odd;
	if ((n & 1<<BPL-2) == 0) {
		if ((odd[-1] & 1<<0) != 0)
			n &= ~ (1 << BPL-1);
		else
			n |= 1 << BPL-1;
		*odd = n;
	}
}


static ulong
chksum(bp, len)
register ulong	*bp;
register uint	len;
{
	register ulong	res;

	assert(len % sizeof(*bp) == 0);
	len /= sizeof(*bp);
	res = 0;
	for (; len != 0; --len)
		res ^= *bp++;
	res = res>>1 & 0x55555555  ^  res & 0x55555555;
	return (res);
}
