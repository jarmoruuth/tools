/**********************************************************************\
 *
 *	VIO.H
 *
 * Video routines.
 *
\**********************************************************************/

#ifndef _VIO_H_
#define _VIO_H_

#define VIO_ATTR(f, b)		((f) | ((b) << 4))
#define VIO_REVATTR(attr)	(((attr) >> 4) | ((attr) << 4))

#define VIO_BLACK		0
#define VIO_BLUE		1
#define VIO_GREEN		2
#define VIO_CYAN		3
#define VIO_RED			4
#define VIO_MAGENTA		5
#define VIO_BROWN		6
#define VIO_LIGHTGRAY		7	/* white on monocrome screen */
#define VIO_DARKGRAY		8
#define VIO_LIGHTBLUE		9
#define VIO_LIGHTGREEN		10
#define VIO_LIGHTCYAN		11
#define VIO_LIGHTRED		12
#define VIO_LIGHTMAGENTA	13
#define VIO_YELLOW		14
#define VIO_WHITE		15

#define VIO_BLINK		0x80
#define VIO_BRIGHT		0x08

void vio_init(void);
int  vio_ismono(void);
void vio_getsize(int* height, int* width);
void vio_writestr(char *str, int len, int row, int col);
void vio_writestr_attr(char *str, int len, int row, int col, int attr);
void vio_writech(char ch, int row, int col);
void vio_writech_attr(char ch, int row, int col, int attr);
void vio_setpos(int row, int col);
void vio_getpos(int* row, int* col);
void vio_scrollup(int n, int r1, int c1, int r2, int c2, int attr);
void vio_scrolldown(int n, int r1, int c1, int r2, int c2, int attr);
void vio_cls(void);

#endif
