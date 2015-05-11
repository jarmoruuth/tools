/**********************************************************************\
 *
 *	LISTMENU.C
 *
 * Utility to display file list in a menu and edit and display it.
 *
\**********************************************************************/

/* Window size and position and select type parameters. In the multi-select
   mode (single_choice == 0) names parameter array is updated to contain
   only selected items.
*/
typedef struct {
	int	row;		/* screen row for window */
	int	col;		/* screen col for window */
	int	width;		/* window width */
	int	height;		/* window height */
	int	single_choice;	/* flag: single- or multi-select menu */
	char*	header;		/* header text at top or NULL */
	char*	keyhelp;	/* key help text at bottom or NULL */
} listmenu_t;

listmenu_t* default_listmenu_t(void);
char* listmenu(char* names[], listmenu_t* l);
