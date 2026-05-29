

extern uint		segment,
			segaddr,
			seglen;
extern struct instr	*segcode;

void	segcreate( ),
	segclose( );
bool	segselect( );
