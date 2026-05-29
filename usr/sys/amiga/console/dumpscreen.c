#define INKERNEL
#include "screen.h"

void dumpscreen(sp)
register struct screen *sp;
{
	printf("flags = 0x%x\n", sp->flags);
	printf("modes = 0x%x\n", sp->modes);
	printf("user = 0x%x\n", sp->user);
	printf("idx = %d\n", sp->idx);
	printf("group = 0x%x\n", sp->group);
	printf("mifunc = 0x%x\n", sp->mifunc);
	printf("kbfunc = 0x%x\n", sp->kbfunc);
	printf("coplist = 0x%x\n", sp->coplist);
	printf("hpos, vpos = 0x%x,0x%x\n", sp->hpos, sp->vpos);
	printf("height, width, depth = %d,%d,%d\n",
	       sp->height, sp->width, sp->depth);
	printf("color = %8x,%8x,%8x,%8x,...\n",
	       sp->color[0], sp->color[1], sp->color[2], sp->color[3]);
	printf("drawmode = 0x%x\n", sp->drawmode);
	printf("fgpen, bgpen = 0x%x,0x%x\n", sp->fgpen, sp->bgpen);
	printf("row, col = %d,%d\n", sp->row, sp->col);
	printf("bmap = 0x%x\n", sp->bmap);
/*
	printf("bmap->flags = 0x%x\n", sp->bmap->flags);
	printf("bmap->width, height, depth, offset = %d,%d,%d,%d\n",
	       sp->bmap->width, sp->bmap->height,
	       sp->bmap->depth, sp->bmap->offset);
	printf("bmap->bpl[0] = 0x%x\n", sp->bmap->bpl[0]);
*/
	printf("kmap = 0x%x\n", sp->kmap);
	printf("font = 0x%x\n", sp->font);
}


int main(argc, argv)
int argc;
char *argv[];
{
	struct screen screen;
	if (read(0, &screen, sizeof screen) != sizeof screen) {
		perror("dumpscreen: error reading standard input");
		return 1;
	}
	dumpscreen(&screen);
	return 0;
}
