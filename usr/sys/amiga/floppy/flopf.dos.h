/*
 * This include file describes the formatting of IBM-compatible 720K
 * floppies, at what would usually be the hardware level.
 */


/*
 * Disk info. which is not dependant on any particular disk format,
 * but which does depend on the density.
 * Note, you shouldn't count on being able to get more than TRKUSED
 * bytes per track, but you might get (and hence must read and write)
 * as many as TRKUSED + TRKSLOP.
 */
#define	RECSLOP		20		/* extra steps for recalibrate */
#define	TRACKS		160		/* number of tracks per disk */
#define	TRKPCYL		2		/* tracks per cylinder */
#define	TRKUSEDL	12028		/* max. bytes to use per track (low) */
#define	TRKSLOPL	1600		/* possible extra bytes on track (low) */
#define	TRKUSEDH	24056		/* max. bytes to use per track (high) */
#define	TRKSLOPH	3200		/* possible extra bytes on track (high) */


/*
 * Information for either 720K or 1440K MS-DOS layout.
 */
#define	BPSEC		512		/* data bytes per sector */
#define	MAXITOD		100		/* max ushorts from dida to dda */


/*
 * Information specific to MS-DOS, 720K layout.
 */
#define	NSECL		9		/* sectors per track */
#define	TRKRLENL	(TRKUSEDL + TRKSLOPL + sizeof(mfmsec))
#define	TRKWLENL	(TRKUSEDL + TRKSLOPL + sizeof(ushort))
#define	TRKBLENL	(TRKRLENL + 16)	/* size of track buff (with slop) */
#define	CHKLENL		(2*sizeof(mfmsec) + TRKSLOPL)


/*
 * Information specific to MS-DOS, 1440K layout.
 */
#define	NSECH		18		/* sectors per track */
#define	TRKRLENH	(TRKUSEDH + TRKSLOPH + sizeof(mfmsec))
#define	TRKWLENH	(TRKUSEDH + TRKSLOPH + sizeof(ushort))
#define	TRKBLENH	(TRKRLENH + 16)	/* size of track buff (with slop) */
#define	CHKLENH		(2*sizeof(mfmsec) + TRKSLOPH)


/*
 * Mfmida describes the layout of the full ida field.  There is one ida per
 * sector.
 *	magic is a bogus MFM encoding of 0xA1A1A1FE.  Instead of encoding to
 *			0x895289528952AAA8 or 0x895289528952AAA9
 *		it is recorded as
 *			0x891289128912AAA8 or 0x891289128912AAA9
 *		I.e., there is a missing clock between the 5th and 6th bits
 *		of each 0xA1.
 *	cyl is the cylinder number in [0, 80).
 *	hd is the head number in [0, 1].
 *	sec is the sector number in [1, 9].
 *	no is always 2.
 */
typedef struct	mfmida {
	ushort	magic[4],
		cyl,
		hd,
		sec,
		no;
}	mfmida;


/*
 * Dida describes the layout of the full ida field after mfm-decoding.
 * There is one ida per sector.
 *	magic is 0xA1A1A1FE.
 *	cyl is the cylinder number in [0, 80).
 *	hd is the head number in [0, 1].
 *	sec is the sector number in [1, 18].
 *	no is always 2.
 *	crc is the check sum.
 */
typedef struct	dida {
	uchar	magic[4],
		cyl,
		head,
		sec,
		no;
	ushort	crc;
}	dida;


/*
 * Mfmda describes the layout of the full da field.  There is one da per
 * sector.
 *	magic is a bogus MFM encoding of 0xA1A1A1FB or 0xA1A1A1F8.  Instead
 *		of encoding to
 *			0x893289328932AA8A or
 *			0x893289328932AA94 or 0x893289328932AA95
 *		it is recorded as
 *			0x891289128912AA8A or
 *			0x891289128912AA94 or 0x891289128912AA95
 *		I.e., there is a missing clock between the 5th and 6th bits
 *		of each 0xA1.
 *	data is the actual sector data.
 */
typedef struct	mfmda {
	ushort	magic[4],
		data[BPSEC];
}	mfmda;


/*
 * Dda describes the layout of the full da field after mfm-decoding.
 * There is one da per sector.
 *	magic is 0xA1A1A1FB or 0xA1A1A1F8
 *	data is the actual sector data.
 */
typedef struct	dda {
	uchar	magic[4],
		data[BPSEC];
	ushort	crc;
}	dda;


/*
 * Mfmsec describes the layout of one mfm-encoded sector in MS-DOS compatible
 * format.
 *	The sync1 field is MFM-encoded 0x00's.
 *	The idacrc is a CRC applied to the ida member.
 *	The gap2 field is 0x4E's.
 *	The sync2 field is 0x00's.
 *	The dacrc is a CRC applied to the da member.
 *	The gap3 field is MFM-encoded 0x4E's.
 */
typedef struct	mfmsec {
	ushort	sync1[12];
	mfmida	ida;
	ushort	idacrc[2],
		gap2[22],
		sync2[12];
	mfmda	da;
	ushort	dacrc[2],
		gap3[80];
}	mfmsec;


/*
 * Mfmia describes the layout of one mfm-encoded ia field.  There is one ia
 * per track.
 *	The sync field is MFM-encoded 0x00's.
 *	magic is a bogus MFM encoding of 0xC2C2C2FC.  Instead of encoding to
 *			0xA548A548A548AAA4 or 0xA548A548A548AAA5
 *		it is recorded as
 *			0xA448A448A448AAA4 or 0xA448A448A448AAA5
 *		I.e., there is a missing clock between the 4th and 5th bits
 *		of each 0xC2.
 *	Gap1 is MFM-encoded 0x4E's.
 */
typedef struct	mfmia {
	ushort	sync[12],
		magic[4],
		gap1[50];
}	mfmia;


/*
 * MFMtrkl describes the layout of one mfm-encoded low density track.
 * MFMtrkh is for a high density track.
 *	Gap4 is MFM-encoded 0xFF's (although sometimes it might be 0x4E's).
 */
typedef struct	mfmtrkl {
	ushort	gap4[80];
	mfmia	ia;
	mfmsec	sec[NSECL];
}	mfmtrkl;

typedef struct	mfmtrkh {
	ushort	gap4[80];
	mfmia	ia;
	mfmsec	sec[NSECH];
}	mfmtrkh;
