/**********************************************************************
 *
 *	MJ.C
 *
 * Unix like file move.
 * Move or rename files in same drive. No disk blocks
 * are moved, only the directory entry. No wild cards 
 * are allowed in the destination.
 *
 * Return values:
 *	0 - ok
 *	1 - no files copied
 *	2 - errors
 *
 *	J.Ruuth 10-09-1987
 *
 **********************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <fcntl.h>
 #include <io.h>
 #include <string.h>
 #include <errno.h>
 #include <process.h>
 #include <conio.h>
 #include <sys\types.h>
 #include <sys\stat.h>
 
 #include <c.h>
 #include <getopt.h>
 #include <listmenu.h>
 #include <diruti.h>
 
 #define MAXPATHLEN	U_MAXPATH
 
 #define DEV		0x01
 #define DIR		0x02
 #define ROOT		0x04
 
 static int	exit_code = 0;		/* exit() code */
 static int	overwrite = 0;		/* overwrite flag */
 static int	force = 0;		/* force flag */
 static int	interactive = 0;	/* interactive flag */
 static int	silent = 0;		/* silent flag */
 
 /**********************************************************************
  *
  *	usage
  *
  * Exits with help message.
  */
 static void usage(int error_code)
 {
     printf(
     "Usage: mj [-fimos] source [source...] destination\n"
     "If more than one source, destination must be directory\n"
     "Options: -f force move\n"
     "         -i interactive\n"
 #ifndef NT
     "         -m use menu to select files\n"
 #endif
     "         -o overwrite existing files\n"
     "         -s silent\n");
     exit(error_code);
 }
 
 /**********************************************************************
  *
  *	mv_error
  *
  * Displays error message.
  */
 static int mv_error(void)
 {
     register char	*error_msg;
     
     exit_code = 2;
     switch (errno) {
         case ENOENT:
             error_msg = "Path or file name not found";
             break;
         case EACCES:
             error_msg = "Permission denied";
             break;
 #if defined(__TURBOC__)
         case ENOTSAM:
 #elif defined(MSC)
             case EXDEV:
 #else
 #error Error
 #endif
             error_msg = "Not same device";
             break;
         default:
             error_msg = "Unknown error";
             break;
     }
     printf("%s\n", error_msg);
     return FALSE;
 }
 
 /**********************************************************************
  *
  *	interact
  *
  * Asks user permission to move file.
  */
 static bool interact(void)
 {
     register int	ans;
     
     fprintf(stderr, " ? ");
     do {
         ans = getch();
         if (ans == 3) {
             fprintf(stderr, "^C\n");
             exit(1);
         }
     } while (!(ans=='y' || ans=='Y' || ans=='n' || ans=='N'));
     if (ans == 'n' || ans == 'N')
         fprintf(stderr, "No\n");
     else
         fprintf(stderr, "Yes\n");
     return ans=='y' || ans=='Y';
 }
 
 /**********************************************************************
  *
  *	overwrite_file
  *
  * Asks user permission to overwrite existing file.
  */
 static bool overwrite_file(char *dest)
 {
     fprintf(stderr, "File %s exists, overwrite", dest);
     return interact();
 }
 
 /**********************************************************************
  *
  *	show_mv
  */
 static void show_mv(char *src, char *dest)
 {
     FILE* f;
     
     if (interactive)
         f = stderr;
     else
         f = stdout;
     fprintf(f, "mv %s %s", src, dest);
 }
 
 /**********************************************************************
  *
  *	mv
  *
  * Moves files. On error, prints error message and returns FALSE,
  * otherwise returns TRUE.
  * Deletes destination before move.
  */
 static bool mv(char *src, char *dest)
 {	
     register int	status;
     
     if (interactive || !silent)
         show_mv(src, dest);
     if (interactive) {
         if (!interact())
             return FALSE;
     } else if (!silent)
          printf("\n");
     if ((status = rename(src, dest)) != 0 && errno == EACCES) {
         if (!interactive && !overwrite && !overwrite_file(dest))
             return FALSE;
         if (force)
             chmod(dest, S_IREAD|S_IWRITE);
         remove(dest);
         status = rename(src, dest);
     }
     if (status != 0) {
         if (silent) {
             show_mv(src, dest);
             printf("\n");
         }
         return mv_error();
     }
     return TRUE;
 }
 
 #ifndef NT
 /**********************************************************************
  *
  *	menu_select
  */
 static bool menu_select (char** files)
 {
     listmenu_t* l;
     char* s;
 
     l = default_listmenu_t();
     l->header = "Select files to move:";
     s = listmenu(files, l);
     fprintf(stderr, "\n");
     return(s != NULL);
 }
 #endif
 
 /**********************************************************************
  *
  *	get_path_type
  */
 static int get_path_type(char *path)
 {
     register int	path_end,
             path_len;
     struct stat	st;
     
     path_end = path[strlen(path)-1];
     if (path_end == ':')
         return DEV;
     path_len = strlen(path[1] == ':' ? path + 2 : path);
     if (path_len == 1 && (path_end == '\\' || path_end == '/'))
         return ROOT;
     if (stat(path, &st) != 0) 	/* not found, must be new file */
         return 0;
     return (st.st_mode & S_IFDIR) ? DIR : 0;
 }
 
 /**********************************************************************
  *
  *	Main
  */
 int main(int argc, register char *argv[])
 {
     static char	destpath[MAXPATHLEN],
             fulldest[MAXPATHLEN];
     int		opt,
             move_count = 0,
             path_type,
             use_menu = 0;
     char*		dest;
     extern int	optind, opterr;
     
     opterr = FALSE;	/* handle errors ourselves */
     while ((opt = getopt(argc, argv, "fimos")) != EOF) {
         switch (opt) {
             case 'f':
                 force++;
                 break;
             case 'i':
                 interactive++;
                 break;
 #ifndef NT
             case 'm':
                 use_menu++;
                 break;
 #endif
             case 'o':
                 overwrite++;
                 break;
             case 's':
                 silent++;
                 break;
             default:
                 printf("Invalid command line option\n");
                 usage(1);
                 break;
         }
     }
     if (argc - optind < 2)
         usage(1);
     dest = argv[argc-1];
     argv[argc-1] = NULL;
 #ifndef NT
     if (use_menu && !menu_select(argv+optind)) {
         printf("No files selected\n");
         return(1);
     }
 #endif
     path_type = get_path_type(strcpy(destpath, dest));
     /* simple first: check if dest doesn't exist or it's a file */
     if (!path_type) {
         if (argc - optind == 2) {
             mv(argv[optind], destpath);
             return(exit_code);
         } else
             usage(1);
     }
     if (path_type == DIR)
         strcat(destpath, "\\");
     /* move files to drive or directory */
     for (; argv[optind]; optind++) {
         strcat(strcpy(fulldest, destpath), u_splitpath(argv[optind]));
         if (mv (argv[optind], fulldest))
             ++move_count;
     }
     if (!move_count) {
         exit_code = max(exit_code, 1);
         if (!silent)
             printf("No files moved\n");
     }
     return(exit_code);
 }
 