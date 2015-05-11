/**********************************************************************\
 *
 *	KBD.H
 *
 * Keyboard routines.
 *
\**********************************************************************/

#ifndef _KBD_H_
#define _KBD_H_

#define KBD_UP		    ('H'<<8)	/* up arrow */
#define KBD_DOWN	    ('P'<<8)	/* down arrow */
#define KBD_LEFT	    ('K'<<8)	/* left arrow */
#define KBD_RIGHT	    ('M'<<8)	/* right arrow */
#define KBD_PGUP	    ('I'<<8)	/* PgUp */
#define KBD_PGDOWN	    ('Q'<<8)	/* PgDn */
#define KBD_HOME	    ('G'<<8)	/* Home */
#define KBD_END		    ('O'<<8)	/* End */
#define KBD_CTRLHOME	('w'<<8)	/* ^Home */
#define KBD_CTRLEND	    ('u'<<8)	/* ^End */
#define KBD_ENTER	    '\r'		/* Enter */
#define KBD_CTRLENTER	'\n'		/* ^Enter */
#define KBD_ESC		    '\x1b'		/* Esc */

int kbd_getkey(void);
int kbd_iskey(void);

#endif
