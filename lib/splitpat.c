/**********************************************************************\
 *
 *	SPLITPAT.C
 *
\**********************************************************************/

/**********************************************************************
 *
 *		u_splitpath
 *
 * Splits given path to directory and file name. Doesn't change
 * path, but returns pointer to start of file name portion.
 */
char* u_splitpath(register char *path)
{
	register char* filebeg = path;
	
	while (*path) {
		switch (*path++) {
			case '\\':
			case ':':
			case '/':
				filebeg = path;
				break;
			case '?':
			case '*':
				return filebeg;
			default:
				break;
		}
	}
	return filebeg;
}
