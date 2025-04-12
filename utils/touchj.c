/**********************************************************************
 *
 *	TOUCH.C
 *
 * Touch files, i.e. set file's time.
 *
 *	J.Ruuth 13-08-1989
 *
 **********************************************************************/

 #include <stdio.h>
 #include <process.h>
 #include <errno.h>
 #include <time.h>
 #include <dos.h>
 #include <io.h>
 #include <conio.h>
 #include <fcntl.h>
 #include <ctype.h>
 
 #include <c.h>
 #include <getopt.h>
 
 /* exit values */
 #define OK		0	/* success */
 #define FILE_ERR	1	/* some files incorrect */
 #define FATAL_ERR	2	/* invalid parameters */
 
 static int		exit_code = OK;
 static int		count = 0;
 static int		interactive = FALSE, verbose = FALSE;
 static struct ftime	ft;
 static struct tm	*tm;
 static char		tmp[128];
 
 /**********************************************************************
  *	usage
  */
 static void usage(int error_code)
 {
     printf(
 "Usage: touch [options] file...\n"
 "Options: -t<time> specifies the time used in touching (format HH.MM[.SS])\n"
 "         -d<date> specifies the date used in touching (format DD-MM[-YY])\n"
 "         -i       interactive\n"
 "         -v       verbose\n"
 "When time (or date) is not specified, current time (or date) is used\n"
 "Following characters can be used as a separator in formats: \".-:\"\n");
     exit(error_code);
 }
 
 /**********************************************************************
  *	fatal
  */
 static void fatal(char *s)
 {
     printf("%s\n", s);
     exit(FATAL_ERR);
 }
 
 /**********************************************************************
  *	error
  */
 static void error(char *fname)
 {
     char *str;
 
     exit_code = FILE_ERR;	
     switch (errno) {
         case EACCES:
             str = "Permission denied";
             break;
         case ENOENT:
             str = "Path or file name not found";
             break;
         case EMFILE:
             str = "Too many open files";
             break;
         case EINVACC:
             str = "Invalid access code";
             break;
         case EINVFNC:
             str = "Invalid function number";
             break;
         case EBADF:
             str = "Bad file number";
             break;
         default:
             str = "Touch failed";
             break;
     }
     printf("%s : %s\n", fname, str);
 }
 
 /**********************************************************************
  *	touch_file_time
  */
 static int touch_file_time(char *file)
 {
     int h, retcode;
 
     h = open(file, O_RDONLY);
     if (h == -1) {
         error(file);
         return FALSE;
     }
     retcode = (setftime(h, &ft) == 0);
     if (retcode == FALSE)
         error(file);
     close(h);
     return retcode;
 }
 
 /**********************************************************************
  *	touch
  */
 static void touch(char *file)
 {
     int c;
 
     if (interactive) {
         fprintf(stderr, "touch %s? ", file);
         c = getch();
         if (c == 0)	/* extended character code */
             getch();
         if (c == 3) {	/* Ctrl-C */
             fprintf(stderr, "^C");
             exit(FILE_ERR);
         }
         c = toupper(c);
         fprintf(stderr, "%s\n", c == 'Y' ? "Yes" : "No");
         if (c != 'Y')
             return;
     }
     if (!touch_file_time(file))
         return;
     count++;
     if (verbose && !interactive)
         printf("%s\n", file);
 }
 
 /**********************************************************************
  *	init_file_time
  */
 static void init_file_time(void)
 {
     ft.ft_tsec = tm->tm_sec / 2;
     ft.ft_min = tm->tm_min;
     ft.ft_hour = tm->tm_hour;
     ft.ft_day = tm->tm_mday;
     ft.ft_month = tm->tm_mon + 1;
     ft.ft_year = tm->tm_year - 80;
 }
 
 /**********************************************************************
  *	set_time
  */
 static void set_time(char *s)
 {
     unsigned hh, mm, ss;
     
     ss = 0;
     if (sscanf(s, "%u %[.:-] %u  %[.:-] %u", &hh, tmp, &mm, tmp, &ss) != 5 &&
         sscanf(s, "%u %[.:-] %u", &hh, tmp, &mm) != 3)
             fatal("Invalid time format");
     if (hh > 23 || mm > 59 || ss > 59)
             fatal("Incorrect time");
     tm->tm_sec = ss;
     tm->tm_min = mm;
     tm->tm_hour = hh;
 }
 
 /**********************************************************************
  *	set_date
  */
 static void set_date(char *s)
 {
     unsigned dd, mm, yy;
     
     yy = tm->tm_year;
     if (sscanf(s, "%u %[.:-] %u  %[.:-] %u", &dd, tmp, &mm, tmp, &yy) != 5 &&
         sscanf(s, "%u %[.:-] %u", &dd, tmp, &mm) != 3)
             fatal("Invalid date format");
     if (yy >= 1900)
         yy -= 1900;
     if (dd > 31 || mm > 12 || yy < 80 || yy > 99)
             fatal("Incorrect date");
     tm->tm_mday = dd;
     tm->tm_mon = mm - 1;
     tm->tm_year = yy;
 }
 
 /**********************************************************************
  *	main
  */
 int main(int argc, char *argv[])
 {
     int	opt;
     time_t	timer;
     
     /* initialize time and date to the current values */
     timer = time(NULL);
     tm = localtime(&timer);
     /* get options */
     opterr = FALSE;	/* handle errors ourselves */
     while ((opt = getopt(argc, argv, "iIvVt:T:d:D:")) != EOF) {
         switch (opt) {
             case 't':
             case 'T':
                 set_time(optarg);
                 break;
             case 'd':
             case 'D':
                 set_date(optarg);
                 break;
             case 'i':
             case 'I':
                 interactive++;
                 break;
             case 'v':
             case 'V':
                 verbose++;
                 break;
             default:
                 usage(FATAL_ERR);
                 break;
         }
     }
     if (verbose)
         printf("Touch time is %s", asctime(tm));
     if (argc - optind < 1)
         usage(FATAL_ERR);
     init_file_time();
     for (; optind < argc; optind++)
         touch(argv[optind]);
     if (verbose)
         printf("%d files touched\n", count);
     return(exit_code);
 }
 