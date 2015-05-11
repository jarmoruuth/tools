/**********************************************************************\
 *
 *	DOSVIO.C
 *
 * Dos specific video routines.
 *
\**********************************************************************/

#include <dos.h>

#include "vio.h"

#define VIO_OFFSET(ro, co)	(80 * (ro) + (co))

typedef enum {
	VM_MONO,
	VM_BW,
	VM_COLOR
} video_mode_t;

static unsigned far * scr_addr;
static int ismono;
static int vio_width = 80, vio_height = 25;

static video_mode_t query_video_mode(void)
{
	union REGS rg;
	
	rg.h.ah = 0x0f;
	int86(0x10, &rg, &rg);
	switch (rg.h.al) {
		case 0x02:
 			return VM_BW;
		case 0x07:
 			return VM_MONO;
 		default:
 			return VM_COLOR;
	}
}

void vio_init(void)
{
	ismono = 0;

	switch (query_video_mode()) {
		case VM_MONO:
			scr_addr = (unsigned far *)0xb0000000L;
			ismono = 1;
			break;
		case VM_BW:
			ismono = 1;
			/* fall trough */
		default:
			scr_addr = (unsigned far *)0xb8000000L;
			break;
	}
}

void vio_getsize(int* height, int* width)
{
	if (height)
		*height = vio_height;
	if (width)
		*width = vio_width;
}

int vio_ismono(void)
{
	return ismono;
}

void vio_writestr(char *str, int len, int row, int col)
{
	register unsigned far *s = scr_addr + VIO_OFFSET(row, col);
	
	for (; *str && len; s++, str++, len--)
		*s = (*s & 0xff00) | *str;

	for (; len; s++, len--)
		*s = (*s & 0xff00) | ' ';
}

void vio_writestr_attr(char *str, int len, int row, int col, int attr)
{
	register unsigned far *s = scr_addr + VIO_OFFSET(row, col);
	
	attr <<= 8;
	
	for (; *str && len; s++, str++, len--)
		*s = attr | *str;

	for (; len; s++, len--)
		*s = attr | ' ';
}

void vio_writech(char ch, int row, int col)
{
	register unsigned far *s = scr_addr + VIO_OFFSET(row, col);
	
	*s = (*s & 0xff00) | ch;
}

void vio_writech_attr(char ch, int row, int col, int attr)
{
	register unsigned far *s = scr_addr + VIO_OFFSET(row, col);
	
	*s = (attr << 8) | ch;
}

void vio_setpos(int row, int col)
{
	union REGS rg;
	
	rg.h.ah = 0x02;		/* function set cursor position */
	rg.h.bh = 0;		/* page number */
	rg.h.dh = row;
	rg.h.dl = col;
	int86(0x10, &rg, &rg);
}

void vio_getpos(int* row, int* col)
{
	union REGS rg;
	
	rg.h.ah = 0x03;		/* function read cursor position */
	rg.h.bh = 0;		/* page number */
	int86(0x10, &rg, &rg);
	if (row)
		*row = rg.h.dh;
	if (col)
		*col = rg.h.dl;
}

void vio_scroll_up(int n, int r1, int c1, int r2, int c2, int attr)
{
	union REGS rg;
	
	rg.h.ah = 0x06;		/* function scroll window up */
	rg.h.al = n;		/* number of lines to scroll */
	rg.h.bh = attr;		/* attribute for blanked area */
	rg.h.ch = r1;
	rg.h.cl = c1;
	rg.h.dh = r2;
	rg.h.dl = c2;
	int86(0x10, &rg, &rg);
}

void vio_scroll_down(int n, int r1, int c1, int r2, int c2, int attr)
{
	union REGS rg;
	
	rg.h.ah = 0x07;		/* function scroll window down */
	rg.h.al = n;		/* number of lines to scroll */
	rg.h.bh = attr;		/* attribute for blanked area */
	rg.h.ch = r1;
	rg.h.cl = c1;
	rg.h.dh = r2;
	rg.h.dl = c2;
	int86(0x10, &rg, &rg);
}

void vio_cls(void)
{
	int h, w;
	
	vio_getsize(&h, &w);
	vio_scroll_up(0, 0, 0, h-1, w-1, VIO_ATTR(VIO_LIGHTGRAY, VIO_BLACK));
}
