/**********************************************************************\
 *
 *	LISTMENU.C
 *
 * Utility to display file list in a menu and edit and display it.
 *
\**********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "vio.h"
#include "kbd.h"

#include "lassert.h"
#include "listmenu.h"

/* colors, FG = forgroung, BG = background */
typedef enum {
	SELECTED = 0,
	UNSELECTED = 1,
	REV_SELECTED = 2,
	REV_UNSELECTED = 3,
	INFOLINE = 4
} mode_t;

typedef struct {
	char*	txt;
	mode_t	mode;
	int	attr;
} menuline_t;

static int attr[5];

/**********************************************************************
 *	init_attr
 */
static void init_attr(void)
{

	if (vio_ismono()) {
		attr[SELECTED] = VIO_ATTR(VIO_WHITE, VIO_BLACK);
		attr[UNSELECTED] = VIO_ATTR(VIO_LIGHTGRAY, VIO_BLACK);
		attr[REV_SELECTED] = VIO_ATTR(VIO_BLACK, VIO_WHITE);
		attr[REV_UNSELECTED] = VIO_ATTR(VIO_BLACK, VIO_LIGHTGRAY);
		attr[INFOLINE] = VIO_ATTR(VIO_LIGHTGRAY, VIO_BLACK);
	} else {
		attr[SELECTED] = VIO_ATTR(VIO_RED, VIO_LIGHTGRAY);
		attr[UNSELECTED] = VIO_ATTR(VIO_BLACK, VIO_LIGHTGRAY);
		attr[REV_SELECTED] = VIO_ATTR(VIO_LIGHTGRAY, VIO_RED);
		attr[REV_UNSELECTED] = VIO_ATTR(VIO_LIGHTGRAY, VIO_BLACK);
		attr[INFOLINE] = VIO_ATTR(VIO_BLUE, VIO_LIGHTGRAY);
	}
}

/**********************************************************************
 *	default_listmenu_t
 */
listmenu_t* default_listmenu_t(void)
{
	static listmenu_t l;

	vio_init();
	vio_getsize(&l.height, &l.width);
	l.row = 0;
	l.col = 0;
	l.single_choice = 0;
	l.header = "Select:";
	l.keyhelp = "Ctrl+Enter Enter=Select Esc=Cancel";
	return &l;
}

/**********************************************************************
 *	listmenu_done
 */
static void listmenu_done(menuline_t* lines, int lastvisible)
{
	/* put cursor to last visible line in menu */
	vio_setpos(lastvisible, 0);
	free(lines);
}

/**********************************************************************
 *	listmenu
 */
char* listmenu(char* names[], listmenu_t* l)
{
	int		i;
	int		j;
	int		row;
	int		col;
	int		width;
	int		height;
	int		nitem;
	menuline_t*	lines;
	int		topline = 0;
	int		winpos = 0;
	int		rev_attr;
	int		nobreak = 0;
	int		lastvisible;
	
	/* hide cursor */
	vio_init();
	vio_getsize(&i, NULL);
	vio_setpos(i, 0);	/* hide cursor */
	
	init_attr();
	
	/* count the number of items in names-array */
	for (nitem = 0; names[nitem]; nitem++)
		;
	lassert(nitem > 0);

	/* allocate array for lines */
	lines = malloc(nitem * sizeof(menuline_t));
	lassert(lines != NULL);
	
	/* init lines-array */
	for (i = 0; i < nitem; i++) {
		lines[i].txt = names[i];
		lines[i].mode = UNSELECTED;
		lines[i].attr = attr[UNSELECTED];
	}
	
	row = l->row;
	height = l->height;
	lastvisible = row + height - 1;
	
	col = l->col;
	width = l->width;
	
	/* write header */
	if (l->header) {
		vio_writestr_attr(
			l->header,
			l->width,
			l->row,
			l->col,
			attr[INFOLINE]);
		row++;
		height--;
	}
	
	/* write key help */
	if (l->keyhelp) {
		vio_writestr_attr(
			l->keyhelp,
			l->width,
			lastvisible,
			l->col,
			attr[INFOLINE]);
		height--;
	}
	
	/*
		Strategy to display menu:
		
		1. Display all visible rows in current window
		2. Redisplay selection line in reverse video
		3. Read keyboard and do action assigned to the key
	*/
	
	for (;;) {
		lassert(winpos >= 0 && winpos < height);
		lassert(topline >= 0 && topline < nitem);
		
		/* display all lines */
		for (i = 0; i < min(height, nitem); i++) {
			vio_writestr_attr(
				lines[topline + i].txt,
				width,
				row + i,
				col,
				lines[topline + i].attr);
		}
		/* display empty lines, if there are not enough lines */
		for (; i < height; i++) {
			vio_writestr_attr(
				"",
				width,
				row + i,
				col,
				attr[UNSELECTED]);
		}
		
		/* build attribute for the current line */
		if (lines[topline + winpos].mode == SELECTED)
			rev_attr = attr[REV_SELECTED];
		else
			rev_attr = attr[REV_UNSELECTED];

		/* display current line */
		vio_writestr_attr(
			lines[topline + winpos].txt,
			width,
			row + winpos,
			col,
			rev_attr);

		/* read keyboard */
		switch (kbd_getkey()) {
			case KBD_UP:
				/* if we are at top of window, update topline,
				   otherwise update just winpos */
				if (winpos == 0) {
					if (topline > 0)
						topline--;
				} else
					winpos--;
				break;
			case KBD_ENTER:
				if (l->single_choice) {
					listmenu_done(lines, lastvisible);
					/* return the current item as selected */
					return lines[topline + winpos].txt;
				}
				nobreak = 1;
			case ' ':
				/* toggle selected/unselected status */
				if (lines[topline + winpos].mode == SELECTED) {
					lines[topline + winpos].mode = 
						UNSELECTED;
					lines[topline + winpos].attr = 
						attr[UNSELECTED];
				} else {
					lines[topline + winpos].mode = 
						SELECTED;
					lines[topline + winpos].attr = 
						attr[SELECTED];
				}
				if (nobreak)
					nobreak = 0;
				else
					break;
				/* if user pressed KBD_ENTER, fall down and
				   do also KBD_DOWN */
			case KBD_DOWN:
				/* if we are at the end of lines, ignore */
				if (topline + winpos + 1 == nitem)
					break;
				/* if we at the end of window, update topline,
				   otherwise update just winpos */
				if (winpos == height - 1)
					topline++;
				else
					winpos++;
				break;
			case KBD_CTRLENTER:
				if (l->single_choice)
					break;
				/* update names array */
				j = 0;
				for (i = 0; i < nitem; i++)
					if (lines[i].mode == SELECTED)
						names[j++] = lines[i].txt;
				names[j] = NULL;
				listmenu_done(lines, lastvisible);
				/* return the current item as selected */
				return lines[topline + winpos].txt;
			case KBD_ESC:
				/* cancel */
				listmenu_done(lines, lastvisible);
				return NULL;
			default:
				/* ignore unrecognized keys */
				break;
		}
	}
}

#ifdef TEST

void main(int argc, char* argv[])
{
	char* selection;
	listmenu_t* l;

	lassert(argc > 1);
	vio_init();
	
	l = default_listmenu_t();
	selection = listmenu(argv+1, l);
	printf("selected %s\n", selection);
}
#endif
