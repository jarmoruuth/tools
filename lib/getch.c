/************************************************************************\
**
**			GETCH.C
**
**	New version of Turbo C library routines getch().
**
\************************************************************************/

#include	<dos.h>
#include	<bios.h>
#include	<conio.h>

/**********************************************************************
 *
 *	getch
 */
int  _Cdecl getch(void)
{
	_AH = 7;
	geninterrupt(0x21);
	return (int)_AL & 0xff;
}
