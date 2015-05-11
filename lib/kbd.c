/**********************************************************************\
 *
 *	KBD.C
 *
 * Keyboard routines.
 *
\**********************************************************************/

#include <conio.h>
#include "kbd.h"

int kbd_getkey(void)
{
	register int ch;
	
	ch = getch();
#if defined(DOS)
	if (ch == 0)
#elif defined(OS2)
	if (ch == 0 || ch == 224)
#else
#error
#endif
		ch = getch() << 8;
	return ch;
}

int kbd_iskey(void)
{
	return kbhit();
}
