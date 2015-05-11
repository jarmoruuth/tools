/**********************************************************************\
 *
 *	OS2VIO.C
 *
 * OS/2 specific video routines.
 *
\**********************************************************************/

#include <stdlib.h>
#include <string.h>

#define INCL_NOPM
#define INCL_VIO
#include <os2.h>

#include "vio.h"

typedef enum {
	VM_MONO,
	VM_BW,
	VM_COLOR
} video_mode_t;

static int ismono;
static int vio_width = 80, vio_height = 25;

static video_mode_t query_video_mode(void)
{
	VIOMODEINFO viomi;

	viomi.cb = sizeof(viomi);
		
	if (VioGetMode(&viomi, (HVIO)0) != 0)
		return VM_COLOR;
	
	vio_width = viomi.col;
	vio_height = viomi.row;
	
	if (viomi.fbType | VGMT_OTHER)
		return VM_COLOR;
	else
		return VM_MONO;
}

void vio_init(void)
{
	switch (query_video_mode()) {
		case VM_MONO:
			ismono = 1;
			break;
		default:
			ismono = 0;
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
	int slen;
	int wrtlen;
	
	slen = strlen(str);
	wrtlen = min(slen, len);
	len -= wrtlen;

	if (wrtlen > 0) {
		VioWrtCharStr((PCH)str, (USHORT)wrtlen, (USHORT)row,
			      (USHORT)col, (HVIO)0);
	}
	
	if (len > 0) {
		char ch = ' ';
		col += wrtlen;
		VioWrtNChar((PCH)&ch, (USHORT)len, (USHORT)row, (USHORT)col,
			    (HVIO)0);
	}
}

void vio_writestr_attr(char *str, int len, int row, int col, int attr)
{
	int slen;
	int wrtlen;
	BYTE byteattr = (BYTE)attr;
	
	slen = strlen(str);
	wrtlen = min(slen, len);
	len -= wrtlen;

	if (wrtlen > 0) {
		VioWrtCharStrAtt((PCH)str, (USHORT)wrtlen, (USHORT)row,
				  (USHORT)col, &byteattr, (HVIO)0);
	}

	if (len > 0) {
		char cell[2];
		cell[0] = ' ';
		cell[1] = byteattr;
		col += wrtlen;
		VioWrtNCell((PCH)cell, (USHORT)len, (USHORT)row, (USHORT)col,
			    (HVIO)0);
	}
}

void vio_writech(char ch, int row, int col)
{
	VioWrtCharStr((PCH)&ch, (USHORT)1, (USHORT)row, (USHORT)col, (HVIO)0);
}

void vio_writech_attr(char ch, int row, int col, int attr)
{
	BYTE byteattr = (BYTE)attr;
	
	VioWrtCharStrAtt((PCH)&ch, (USHORT)1, (USHORT)row, (USHORT)col,
			  &byteattr, (HVIO)0);
}

void vio_setpos(int row, int col)
{
	VioSetCurPos((USHORT)row, (USHORT)col, (HVIO)0);
}

void vio_getpos(int* row, int* col)
{
	USHORT r, c;
	
	VioGetCurPos(&r, &c, (HVIO)0);
	
	if (row)
		*row = r;
	if (col)
		*col = c;
}

void vio_scroll_up(int n, int r1, int c1, int r2, int c2, int attr)
{
	char cell[2];
	
	cell[0] = ' ';
	cell[1] = (char)attr;
	
	VioScrollUp((USHORT)r1, (USHORT)c1, (USHORT)r2, (USHORT)c2, (USHORT)n,
		    cell, (HVIO)0);
}

void vio_scroll_down(int n, int r1, int c1, int r2, int c2, int attr)
{
	char cell[2];
	
	cell[0] = ' ';
	cell[1] = (char)attr;
	
	VioScrollDn((USHORT)r1, (USHORT)c1, (USHORT)r2, (USHORT)c2, (USHORT)n,
		    cell, (HVIO)0);
}

void vio_cls(void)
{
	int h, w;
	
	vio_getsize(&h, &w);
	vio_scroll_up(0, 0, 0, -1, -1, VIO_ATTR(VIO_LIGHTGRAY, VIO_BLACK));
}
