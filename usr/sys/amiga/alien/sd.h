

/*
 * general SCSI service
 */

#define	sdspl	spl2
#define	SDCARDS	2
#define	SDUNITS	8

#define DIOC			'DIOC'
#define DIOCHARDWARENAME	'DCHW'

/*
 * A note on minor device numbers: `sdpart' identifies bits used for
 * partitioning.  SCSI drivers that don't partition may use this field for
 * other purposes.  Fields defined by `sdcard' & `sdunit' are optional, but
 * recommended.
 * For the moment, the bottom 8 bits are used as the "MINOR" number even
 * though Unix uses more.
 */
#define MINOR( dev)	((dev) & 0xFF)

#define	sdunit( dev)	((dev)>>0 & 07)
#define	sdcard( dev)	((dev)>>3 & 01)
#define	sdpart( dev)	((dev)>>4 & 07)
#define	sddev0p( dev)	((dev) & ~(07<<4))

#define	byte3( i)	((uint)(i) >> 3*8 & 0xFF)
#define	byte2( i)	((uint)(i) >> 2*8 & 0xFF)
#define	byte1( i)	((uint)(i) >> 1*8 & 0xFF)
#define	byte0( i)	((uint)(i) >> 0*8 & 0xFF)


struct sdcom {
	volatile struct sdcom	*next;
	bool		reading,
			okay;
	uchar		status,
			cdb[12];
	caddr_t		addr;
	uint		nbyte,
			card,
			unit;
	void		(*intr)( );
};


int	sdopen( ),
	sdpartition( );
bool	sdvalid( );
uint	sdblkno( ),
	sddevsize( );
char *	sdhardwarename( );
