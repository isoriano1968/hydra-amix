/*
 * This file contains the routines which do track I/O on disks
 * formatted as 720K or 1.44 megabyte MS-DOS floppies.
 */
#include "hstyle.h"
#include "flopf.dos.h"


#define	RETRY	5


/*
 * Functions that drive the hardware.
 */
extern void	flinit(),		/* initialize hardware */
		flstop(),		/* turn motor off */
		fllost();		/* issue recal next operation */
extern bool	flread(),		/* read a track */
		fliwrite();		/* write a track INDEX sync'd */


/*
 * Externally visible functions.
 */
extern void	fdmstbuf();		/* set chip-memory track buffer */
extern bool	fdgetmsl(),		/* read MS-DOS low density track */
		fdgetmsh(),		/* read MS-DOS high density track */
		fdputmsl(),		/* write MS-DOS low density track */
		fdputmsh();		/* write MS-DOS high density track */


/*
 * Local functions.
 */
static bool	gettrk(),		/* read a track */
		get(),			/* try to read track */
		findidam(),		/* find id address mark */
		findddam(),		/* find data address mark */
		search(),		/* search for pattern */
		checkeq(),		/* check bits of mfm-stream */
		demfm();		/* strip mfm clock bits */
static void	makedemtab();		/* build mfm-stripping table */
static uint	crc();			/* compute CRC of a buffer */
static void	makecrc();		/* build CRC table */
static bool	put();			/* write a track */
static void	encode();		/* mfm-encode a track */
static bool	check(),		/* check if we are on track */
		chktrk();		/* try to check if we are on track */
static void	dosec(),		/* encode a sector of data */
		mfm(),			/* mfm-encode a buffer */
		makemfmtab(),		/* build mfm-encoding buffer */
		mfmrep(),		/* repeat mfm-encoded byte */
		mfmcopy();		/* copy data to mfm stream */


/*
 * Local variables.
 */
static uchar	*trkbuf;		/* chip memory track buffer */
static uint	drive,			/* drive we are working on */
		track;			/* track we are working on */
static uchar	(*buffer)[BPSEC];	/* user buffer */


/*
 * A position in the mfm-encoded stream is represented by a (ushort)
 * pointer and a bit skip.  The bit skip is in [0, bitsof(ushort)).
 * The next ushort of data is
 *	mfmnext[0] << mfmbskip | mfmnext[1] >> bitsof(ushort)-mfmbskip
 * anded with maskover(bitsof(ushort)).
 * Note, if mfmbskip is 0, then the above may fetch one word too many.
 */
static ushort	*mfmnext,
		*mfmlim;
static uint	mfmbskip;


/*
 * Ammagic contains a table of the first ushort-worth of mfm-encoded
 * bits used in both id and data address marks, with various amounts
 * of mfm-encoded 0's shifted in on the left.
 * (The magic bit pattern is 0xA1A1 with a missing clock between bits
 * 2 and 3.)
 */
static ushort	ammagic[bitsof(ushort)] = {
			0x8912,		/* 0 bits skipped */
			0x4489,		/* 1 bit skipped */
			0x2244,		/* 2 bits skipped */
			0x9122,		/* 3 bits skipped */
			0x4891,		/* 4 bits skipped */
			0xa448,		/* 5 bits skipped */
			0x5224,		/* 6 bits skipped */
			0xa912,		/* 7 bits skipped */
			0x5489,		/* 8 bits skipped */
			0xaa44,		/* 9 bits skipped */
			0x5522,		/* 10 bits skipped */
			0xaa91,		/* 11 bits skipped */
			0x5548,		/* 12 bits skipped */
			0xaaa4,		/* 13 bits skipped */
			0x5552,		/* 14 bits skipped */
			0xaaa9		/* 15 bits skipped */
		};


/*
 * Set chip memory track buffer.
 * Note, the buffer must be atleast TRKBLEN long.
 */
void
fdmstbuf(bp, size)
uchar	*bp;
int	size;
{
	assert(TRKBLENH >= TRKBLENL);
	if (size < TRKBLENH)
		panic("Floppy chip buffer too small (%d < %d)\n",
			size, TRKBLENH);
	trkbuf = bp;
}


/*
 * Read in a low-density track.  Buff must point to an area which is
 * NSECL * BPSEC bytes long.
 */
bool
fdgetmsl(drv, trk, buff)
uint	drv,
	trk;
char	*buff;
{
	drive = drv;
	track = trk;
	buffer = (uchar (*)[BPSEC])buff;
	return (gettrk(NSECL, TRKRLENL));
}


/*
 * Read in a high-density track.  Buff must point to an area which is
 * NSECH * BPSEC bytes long.
 */
bool
fdgetmsh(drv, trk, buff)
uint	drv,
	trk;
char	*buff;
{
	drive = drv;
	track = trk;
	buffer = (uchar (*)[BPSEC])buff;
	return (gettrk(NSECH, TRKRLENH));
}


/*
 * Read in a track.  This includes (bit) aligning the track, MFM
 * decoding and check-summing.  Nsec is the number of sectors to
 * get and trkrlen is the number of bytes to read raw.
 */
bool
gettrk(nsec, trkrlen)
uint	nsec,
	trkrlen;
{
	register uint	n;

	for (n = RETRY; not get(nsec, trkrlen); --n)
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
get(nsec, trkrlen)
uint	nsec,
	trkrlen;
{
	uint		sec;
	bool		gotit[NSECH];
	dida		ida;
	static dda	da;		/* conserve u-area stack space */

	assert(cardof(gotit) >= nsec);
	if (not flread(drive, track, trkbuf, trkrlen))
		return (FALSE);		/* read failure */
	for (sec = 0; sec != nsec; ++sec)
		gotit[sec] = FALSE;
	mfmnext = (ushort *)trkbuf;
	mfmlim = (ushort *)(trkbuf + trkrlen);
	mfmbskip = 0;
	while (findidam(&ida)) {
		unless ((0 <= ida.cyl && ida.cyl < TRACKS/TRKPCYL)
		and (0 <= ida.head && ida.head < TRKPCYL)
		and (track == ida.cyl*TRKPCYL + ida.head)
		and (0 < ida.sec && ida.sec <= nsec))
			return (FALSE);
		if (gotit[ida.sec - 1])
			continue;
		unless (findddam(&da))
			continue;
		gotit[ida.sec - 1] = TRUE;
		bcopy(da.data, buffer[ida.sec - 1], BPSEC);
	}
	for (sec = 0; sec != nsec; ++sec)
		unless (gotit[sec])
			return (FALSE);
	return (TRUE);
}


/*
 * Find the next id address mark (if any) and read it in.
 * The mark consists of 12-bytes of (mfm-encoded) zeros followed by an
 * mfm-encoded 0xA1A1A1FE where the 0xA1's have funny mfm-encoding (a 0
 * clock bit between bits 2 and 3).
 * If an address mark is found, the dida pointed to by idap is filled in,
 * the mfm stream is advanced past the id data and TRUE is returned.
 * If not, FALSE is returned and the stream is left positioned at the end.
 */
static bool
findidam(idap)
dida	*idap;
{
	static ushort	iam[]	= {
				0x8912,	0x8912,	0x8912,	0xAAA8
			};

	while (search(ammagic, iam, bitsof(iam) - 1, 0))
		if ((demfm((uchar *)idap, sizeof(*idap)))
		and (crc((uchar *)idap, sizeof(*idap)) == 0))
			return (TRUE);
	return (FALSE);
}


/*
 * Find the next data address mark (if any) and read in the sector.
 * The mark consists of 12-bytes of (mfm-encoded) zeros followed by an
 * mfm-encoded 0xA1A1A1FB or 0xA1A1A1F8 where the 0xA1's have funny
 * mfm-encoding (a 0 clock bit between bits 2 and 3).
 * If a data address mark is found, the dda pointed to by ddap is filled in,
 * and the mfm stream is advanced past the id data.  If the crc is correct,
 * then TRUE is returned.
 */
static bool
findddam(ddap)
dda	*ddap;
{
	static ushort	dam[]	= {
				0x8912,	0x8912,	0x8912,	0xAA80
			};

	unless ((search(ammagic, dam, bitsof(dam)-5, MAXITOD))
	and (demfm((uchar *)ddap, sizeof(*ddap))))
		return (FALSE);
	unless (crc((uchar *)ddap, sizeof(*ddap)) == 0)
		return (FALSE);
	return (TRUE);
}


/*
 * Search advances the mfm-encoded stream until it finds a given pattern
 * prefixed by mfm-encoded 0's.  Mtab is a table of bitsof(ushort)
 * ushorts, indicating the first word of the pattern with successively
 * more mfm-encoded 0's shifted in from the left.  Pat is a table of
 * ushorts which contains the actual pattern being looked for.  Patlen
 * is the length of pat in bits.  The last ushort in pat must be left
 * justified and zero filled.  Maxsrch is the maximum number of ushorts
 * to search.  Note, if maxsrch is 0, then the search is unrestricted.
 * If search succeeds, then the mfm stream is left at the start of the
 * pattern and TRUE is returned.  If not, the stream is left at the end
 * and FALSE is returned.
 */
static bool
search(mtab, pat, patlen, maxsrch)
ushort	*mtab,
	*pat;
uint	patlen,
	maxsrch;
{
	uint		w;
	ushort		*mtp,
			*olim;

	assert(mfmnext <= mfmlim);
	if ((mfmbskip != 0)
	and (mfmnext != mfmlim))
		++mfmnext;
	olim = mfmlim;
	if ((maxsrch != 0)
	and (mfmlim > mfmnext + maxsrch))
		mfmlim = mfmnext + maxsrch;
	while (mfmnext != mfmlim) {
		w = *mfmnext++;
		unless ((w == 0xAAAA || w == 0x5555 || w == 0x5554)
		and (mfmnext != mfmlim))
			continue;
		w = *mfmnext;
		mtp = &mtab[bitsof(ushort) - 1];
		do {
			if (w == *mtp) {
				mfmbskip = mtp - mtab;
				if (checkeq(pat, patlen)) {
					mfmlim = olim;
					return (TRUE);
				}
			}
		} until (mtp-- == mtab);
	}
	mfmlim = olim;
	return (FALSE);
}


/*
 * Check the next `bitlen' bits of the mfm stream to see if they match
 * `goal'.
 * Note, the stream is NOT advanced.
 * Note, the last ushort of goal is left justified, zero filled.
 */
static bool
checkeq(goal, bitlen)
ushort	*goal;
uint	bitlen;
{
	ushort	*mp;
	uint	buse;
	ushort	w;

	assert(0 <= mfmbskip && mfmbskip < bitsof(ushort));
	assert(mfmnext <= mfmlim);
	if (bitlen == 0)
		return (TRUE);
	if (bitlen + mfmbskip > (mfmlim - mfmnext) * bitsof(ushort))
		return (FALSE);
	mp = mfmnext;
	buse = bitsof(ushort) - mfmbskip;
	while (bitlen > bitsof(ushort)) {
		bitlen -= bitsof(ushort);
		w = *mp++;
		if (mfmbskip != 0)
			w = w<<mfmbskip & maskover(bitsof(ushort))
				| *mp>>buse;
		if (*goal++ != w)
			return (FALSE);
	}
	w = *mp++ << mfmbskip;
	if ((mfmbskip != 0)
	and (bitlen > buse))
		w |= *mp >> buse;
	w &= ~ maskover(bitsof(ushort) - bitlen);
	return (w == *goal);
}


/*
 * Strip the mfm clock bits from the mfm stream, storing the resulting
 * data in [dst, dst + dlen).  It returns TRUE unless the mfm stream
 * didn't have enough data.
 */
static bool
demfm(dst, dlen)
uchar	*dst;
uint	dlen;
{
	ushort		w;
	uint		left,
			buse;
	static bool	first	= TRUE;
	static uchar	demhtab[1<<bitsof(uchar)],
			demltab[1<<bitsof(uchar)];

	if (first) {
		first = FALSE;
		makedemtab(&demhtab, &demltab);
	}
	assert(0 <= mfmbskip && mfmbskip < bitsof(ushort));
	assert(mfmnext <= mfmlim);
	if (mfmbskip == 0) {
		if (dlen > mfmlim - mfmnext)
			return (FALSE);
		while (dlen-- != 0) {
			w = *mfmnext++;
			*dst++ = demhtab[w >> bitsof(uchar)]
				| demltab[w & maskover(bitsof(uchar))];
		}
	} else {
		if (dlen >= mfmlim - mfmnext)
			return (FALSE);
		left = *mfmnext++;
		buse = bitsof(ushort) - mfmbskip;
		while (dlen-- != 0) {
			w = left<<mfmbskip & maskover(bitsof(ushort));
			left = *mfmnext++;
			w |= left >> buse;
			*dst++ = demhtab[w >> bitsof(uchar)]
				| demltab[w &maskover(bitsof(uchar))];
		}
	}
	return (TRUE);
}


/*
 * Fill in the mfm-stripping table.
 */
static void
makedemtab(htab, ltab)
uchar	*htab,
	*ltab;
{
	uint	ibit,
		obit,
		ch,
		res;

	for (ch = 0; ch != 1<<bitsof(uchar); ++ch) {
		res = 0;
		ibit = 1 << bitsof(uchar)-1;
		obit = 1 << bitsof(uchar)/2 - 1;
		do {
			if (ch & ibit)
				res |= obit;
			ibit >>= 2;
			obit >>= 1;
		} while (obit != 0);
		htab[ch] = res << bitsof(uchar)/2;
		ltab[ch] = res;
	}
}


/*
 * Compute the CRC of a byte array.
 */
static uint
crc(fwa, len)
uchar	*fwa;
uint	len;
{
	uchar		hcrc,
			lcrc,
			tmp;
	static bool	first	= TRUE;
	static uchar	htab[1 << bitsof(uchar)],
			ltab[1 << bitsof(uchar)];

	if (first) {
		first = FALSE;
		makecrc(htab, ltab);
	}
	hcrc = 0xFF;
	lcrc = 0xFF;
	while (len-- != 0) {
		tmp = hcrc ^ *fwa++;
		hcrc = htab[tmp] ^ lcrc;
		lcrc = ltab[tmp];
	}
	return (hcrc << bitsof(uchar) | lcrc);
}


/*
 * Fill in the high and low byte CRC table.
 */
static void
makecrc(htab, ltab)
uchar	*htab,
	*ltab;
{
	uint	byte,
		res,
		bit;

	for (byte = 0; byte != 1<<bitsof(uchar); ++byte) {
		res = 0;
		for (bit = 1<<bitsof(uchar)-1; bit != 0; bit >>= 1) {
			res <<= 1;
			if (res & 1<<bitsof(ushort))
				res ^= 0x11021;
			if (byte & bit)
				res ^= 0x01021;
		}
		*htab++ = res >> bitsof(uchar);
		*ltab++ = res & maskover(bitsof(uchar));
	}
}


/*
 * Write out a track in MS-DOS low-density format.
 * Buff must point to an area which is NSECL * BPSEC bytes long.
 * If `chk' is TRUE, we first check to make sure that we are
 * on track.
 */
bool
fdputmsl(drv, trk, buff, chk)
uint	drv,
	trk;
char	*buff;
bool	chk;
{
	drive = drv;
	track = trk;
	if (chk)
		if (not check(CHKLENL))
			return (FALSE);
	encode(buff, NSECL, TRKWLENL, sizeof(mfmtrkl));
	return (put(TRKWLENL));
}


/*
 * Write out a track in MS-DOS high-density format.
 * Buff must point to an area which is NSECH * BPSEC bytes long.
 * If `chk' is TRUE, we first check to make sure that we are
 * on track.
 */
bool
fdputmsh(drv, trk, buff, chk)
uint	drv,
	trk;
char	*buff;
bool	chk;
{
	drive = drv;
	track = trk;
	if (chk)
		if (not check(CHKLENH))
			return (FALSE);
	encode(buff, NSECH, TRKWLENH, sizeof(mfmtrkh));
	return (put(TRKWLENH));
}


/*
 * Actually write out an mfm-encoded track.
 */
static bool
put(trkwlen)
uint	trkwlen;
{
	uint	n;

	for (n = RETRY; not fliwrite(drive, track, mfmnext, trkwlen); --n)
		if (n == 0) {
			printf("Floppy: unrecoverable write error\n");
			return (FALSE);
		} else
			printf("Floppy: write error (retrying)\n");
	return (TRUE);
}


/*
 * Encode a tracks worth of data.
 * Sizemfmtrk should be either sizeof(mfmtrkl) or
 * sizeof(mfmtrkh).
 */
static void
encode(buff, nsec, trkwlen, size_mfmtrk)
char	*buff;
uint	nsec,
	trkwlen,
	size_mfmtrk;
{
	uint		sec;
	static ushort	iamagic[]	= {
				0xA448,	0xA448,	0xA448,	0xAAA4
			};

	buffer = (uchar (*)[BPSEC])buff;
	mfmnext = (ushort *)trkbuf;
	*mfmnext++ = 0;
	mfmrep(0x4E, trkwlen/sizeof(*mfmnext));
	assert(mfmnext == (ushort *)(trkbuf + trkwlen) + 1);
	mfmnext -= size_mfmtrk / sizeof(*mfmnext);
	assert((ushort *)trkbuf < mfmnext);
	assert(cardof(((mfmtrkl *)0)->gap4) == cardof(((mfmtrkh *)0)->gap4))
	mfmrep(0x4E, cardof(((mfmtrkl *)0)->gap4));
	mfmrep(0x00, cardof(((mfmia *)0)->sync));
	mfmcopy(iamagic, cardof(iamagic));
	mfmrep(0x4E, cardof(((mfmia *)0)->gap1));
	for (sec = 0; sec < nsec; ++sec)
		dosec(track/TRKPCYL, track%TRKPCYL, sec + 1, &buffer[sec]);
	assert(mfmnext == (ushort *)(trkbuf + trkwlen) + 1);
	mfmnext = (ushort *)trkbuf + 1;
}


/*
 * Check to make sure that we are on track.
 */
static bool
check(chklen)
uint	chklen;
{
	int	n;

	for (n = RETRY; not chktrk(chklen); --n)
		if (n == 0) {
			printf("Floppy: unrecoverable seek error\n");
			return (FALSE);
		} else
			printf("Floppy: seek error (retrying)\n");
	return (TRUE);
}


/*
 * Chktrk checks to make sure that we are on track before writing.
 */
static bool
chktrk(chklen)
uint	chklen;
{
	dida	ida;

	unless (flread(drive, track, trkbuf, chklen))
		return (FALSE);
	mfmnext = (ushort *)trkbuf;
	mfmlim = (ushort *)(trkbuf + chklen);
	mfmbskip = 0;
	if ((findidam(&ida))
	and (ida.cyl == track/TRKPCYL)
	and (ida.head == track%TRKPCYL))
		return (TRUE);
	fllost(drive);
	return (FALSE);
}


/*
 * Encode a sector of data.
 * Note, this version copyies the un-encoded data, which is rather
 * inefficient.
 */
static void
dosec(cyl, head, sec, data)
uint	cyl,
	head,
	sec;
uchar	*data;
{
	int		i;
	static dida	ida	= {
				{ 0xA1,	0xA1,	0xA1,	0xFE },	/* magic */
				0x00,				/* ? cyl */
				0x00,				/* ? head */
				0x00,				/* ? sec */
				0x02,				/* no */
				0x0000				/* ? crc */
			};
	static dda	da	= {
				{ 0xA1,	0xA1,	0xA1,	0xFB },	/* magic */
				{}				/* ? data */
			};
	static ushort	idamagic[]	= {
				0x8912,	0x8912,	0x8912,	0xAAA8
			},
			damagic[]	= {
				0x8912,	0x8912, 0x8912,	0xAA8A
			};

	mfmrep(0x00, cardof(((mfmsec *)0)->sync1));
	ida.cyl = cyl;
	ida.head = head;
	ida.sec = sec;
	ida.crc = crc((uchar *)&ida, sizeof(ida) - sizeof(ida.crc));
	mfmcopy(idamagic, cardof(idamagic));
	mfm((uchar *)&ida + sizeof(ida.magic), sizeof(ida)-sizeof(ida.magic));
	mfmrep(0x4E, cardof(((mfmsec *)0)->gap2));
	mfmrep(0x00, cardof(((mfmsec *)0)->sync2));
	mfmcopy(damagic, cardof(damagic));
	for (i = 0; i != cardof(da.data); ++i)
		da.data[i] = data[i];
	da.crc = crc((uchar *)&da, sizeof(da) - sizeof(da.crc));
	mfm((uchar *)&da + sizeof(da.magic), sizeof(da)-sizeof(da.magic));
	mfmrep(0x4E, cardof(((mfmsec *)0)->gap3));
}


/*
 * Mfm-encode the `len' bytes starting at `from' into the mfm-encoded
 * stream.  Note, this assumes that mfmnext[-1] is valid and has a final
 * (least significant) clock bit of 0.  It adjusts that clock bit to a 1
 * if needed.  Also, it leaves mfmnext[-1] with a final clock bit of 0.
 */
static void
mfm(from, len)
uchar	*from;
uint	len;
{
	uint		last,
			ch;
	static bool	first	= TRUE;
	static ushort	mtab[1 << bitsof(uchar)];

	if (first) {
		first = FALSE;
		makemfmtab(mtab);
	}
	last = mfmnext[-1] & 1<<1;		/* last data bit */
	while (len-- != 0) {
		ch = *from++;
		if ((last == 0)
		and ((ch & 1<<bitsof(uchar)-1) == 0))
			mfmnext[-1] |= 1<<0;
		last = ch & 1<<0;
		*mfmnext++ = mtab[ch];
	}
}


/*
 * Fill in a table of mfm-encoded bytes.
 * Note, the final (least significant) bit in the table entries is always
 * 0.
 */
static void
makemfmtab(mtp)
ushort	*mtp;
{
	uint	ch,
		res,
		bit;

	for (ch = 0; ch != 1 << bitsof(uchar); ++ch) {
		res = 0;
		bit = 1 << bitsof(uchar)-1;
		do {
			res <<= 2;
			if (ch & bit) {
				res |= 1<<1 | 0<<0;
				bit >>= 1;
			} else {
				bit >>= 1;
				unless (ch & bit)
					res |= 0<<1 | 1<<0;
			}
		} until (bit == 0);
		res &= ~ (1 << 0);
		*mtp++ = res;
	}
}


/*
 * Mfm-encode `cnt' copies of `byte' into the mfm-encoded stream.
 */
static void
mfmrep(byte, cnt)
uchar	byte;
uint	cnt;
{
	uchar	buff[1];
	ushort	val;

	if (cnt == 0)
		return;
	buff[0] = byte;
	mfm(buff, 1);
	if (--cnt == 0)
		return;
	mfm(buff, 1);
	val = mfmnext[-1] = mfmnext[-2];
	while (--cnt != 0)
		*mfmnext++ = val;
	mfmnext[-1] &= ~ (1<<0);
}


/*
 * Given some already-mfm-encoded data, add it to the mfm-encoded stream.
 */
static void
mfmcopy(from, cnt)
ushort	*from;
uint	cnt;
{
	if (cnt == 0)
		return;
	if (((mfmnext[-1] & 1<<1) == 0)
	and ((*from & 1<<bitsof(ushort)-1) == 0))
		mfmnext[-1] |= 1<<0;
	do {
		*mfmnext++ = *from++;
	} until (--cnt == 0);
}
