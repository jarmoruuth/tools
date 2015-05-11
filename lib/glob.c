/* THIS IS A MODIFIED VERSION
   See note below.

   File-name wildcard pattern matching for GNU.
   Copyright (C) 1985 Free Software Foundation, Inc.

		       NO WARRANTY

  BECAUSE THIS PROGRAM IS LICENSED FREE OF CHARGE, WE PROVIDE ABSOLUTELY
NO WARRANTY, TO THE EXTENT PERMITTED BY APPLICABLE STATE LAW.  EXCEPT
WHEN OTHERWISE STATED IN WRITING, FREE SOFTWARE FOUNDATION, INC,
RICHARD M. STALLMAN AND/OR OTHER PARTIES PROVIDE THIS PROGRAM "AS IS"
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY
AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE PROGRAM PROVE
DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR
CORRECTION.

 IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW WILL RICHARD M.
STALLMAN, THE FREE SOFTWARE FOUNDATION, INC., AND/OR ANY OTHER PARTY
WHO MAY MODIFY AND REDISTRIBUTE THIS PROGRAM AS PERMITTED BELOW, BE
LIABLE TO YOU FOR DAMAGES, INCLUDING ANY LOST PROFITS, LOST MONIES, OR
OTHER SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE
USE OR INABILITY TO USE (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR
DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY THIRD PARTIES OR
A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS) THIS
PROGRAM, EVEN IF YOU HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, OR FOR ANY CLAIM BY ANY OTHER PARTY.

		GENERAL PUBLIC LICENSE TO COPY

  1. You may copy and distribute verbatim copies of this source file
as you receive it, in any medium, provided that you conspicuously and
appropriately publish on each copy a valid copyright notice "Copyright
(C) 1985 Free Software Foundation, Inc."; and include following the
copyright notice a verbatim copy of the above disclaimer of warranty
and of this License.

  2. You may modify your copy or copies of this source file or
any portion of it, and copy and distribute such modifications under
the terms of Paragraph 1 above, provided that you also do the following:

    a) cause the modified files to carry prominent notices stating
    that you changed the files and the date of any change; and

    b) cause the whole of any work that you distribute or publish,
    that in whole or in part contains or is a derivative of this
    program or any part thereof, to be licensed at no charge to all
    third parties on terms identical to those contained in this
    License Agreement (except that you may choose to grant more extensive
    warranty protection to some or all third parties, at your option).

    c) You may charge a distribution fee for the physical act of
    transferring a copy, and you may at your option offer warranty
    protection in exchange for a fee.

Mere aggregation of another unrelated program with this program (or its
derivative) on a volume of a storage or distribution medium does not bring
the other program under the scope of these terms.

  3. You may copy and distribute this program (or a portion or derivative
of it, under Paragraph 2) in object code or executable form under the terms
of Paragraphs 1 and 2 above provided that you also do one of the following:

    a) accompany it with the complete corresponding machine-readable
    source code, which must be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    b) accompany it with a written offer, valid for at least three
    years, to give any third party free (except for a nominal
    shipping charge) a complete machine-readable copy of the
    corresponding source code, to be distributed under the terms of
    Paragraphs 1 and 2 above; or,

    c) accompany it with the information you received as to where the
    corresponding source code may be obtained.  (This alternative is
    allowed only for noncommercial distribution and only if you
    received the program in object code or executable form alone.)

For an executable file, complete source code means all the source code for
all modules it contains; but, as a special exception, it need not include
source code for modules which are standard libraries that accompany the
operating system on which the executable file runs.

  4. You may not copy, sublicense, distribute or transfer this program
except as expressly provided under this License Agreement.  Any attempt
otherwise to copy, sublicense, distribute or transfer this program is void and
your rights to use the program under this License agreement shall be
automatically terminated.  However, parties who have received computer
software programs from you with this License Agreement will not have
their licenses terminated so long as such parties remain in full compliance.


In other words, you are welcome to use, share and improve this program.
You are forbidden to forbid anyone else to use, share and improve
what you give them.   Help stamp out software-hoarding!  */


/* To whomever it may concern: I have never seen the code which most
 Unix programs use to perform this function.  I wrote this from scratch
 based on specifications for the pattern matching.  */

/**********************************************************************\
 *
 *   Ported to MS-DOS.
 *
 * Made file name matching case insensitive, removed directory
 * search routine and some minor patches. Also fixed bugs in
 * character class matching.
 *
 *  	J.Ruuth 27-Mar-1988
\**********************************************************************/

#if 0
#include <sys/types.h>
#include <sys/dir.h>
#endif
#ifdef ECTYPE
#include <ectype.h>
#else
#include <ctype.h>
#endif

static int glob_match_after_star ();


/**********************************************************************
 *
 *	glob_pattern_p
 *
 * Return nonzero if PATTERN has any special globbing chars in it.
 */

int
glob_pattern_p (pattern)
     register char *pattern;
{
  register int c;

  while ((c = *pattern++))
    {
      switch (c)
	{
	case '?':
	case '[':
	case '*':
	  return 1;

	case '\\':
	  if (*pattern++ == 0) return 0;
	  break;
	default:
	  break;
	}
    }
  return 0;
}


/**********************************************************************
 *
 *	glob_match
 *
 * compare returns 1 if there is a match, 0 if not.
 * The entire string text must be matched by the pattern.
 * In the pattern string, * matches any sequence of characters, ? matches
 * any character, [set] matches any character in the specified set,
 * [^set] matches any character not in the specified set.
 * A set is composed of characters or ranges; a range looks like
 * character hyphen character (as in 0-9 or A-Z).
 * [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
 * Any other character in the pattern must be matched exactly.
 * To suppress the special syntactic significance of [, ], *, ?, ^, - or \,
 * and match the character exactly, precede it with a \.
 */

int
glob_match (pattern, text)
     register char *pattern, *text;
{
  register char c;

#if 0
  printf("glob_match: pattern = '%s'\n text = '%s'\n",pattern,text);
#endif

  while ((c = toupper(*pattern)))
    {
      ++pattern;
      switch (c)
	{
	case '?':
	  if (*text++ == 0) return 0;
	  break;

	case '\\':
	  if (toupper(*pattern) != toupper(*text)) return 0;
	  ++pattern;
	  ++text;
	  break;

	case '*':
	  return glob_match_after_star (pattern, text);

	case '[':
	  {
	    register char c1 = toupper(*text);
	    register int invert = (*pattern == '^');

	    ++text;

	    if (invert) pattern++;

	    if (!(c = *pattern++)) return 0;
	    while (1)
	      {
		register char cstart = c, cend = c;

		if (c == '\\')
		    if (!(cstart = cend = *pattern++)) return 0;
		if (!(c = *pattern++)) return 0;
		if (c == '-')
		  { 
		    if (!(cend = *pattern++)) return 0;
		    if (cend == '\\')
		    	if (!(cend = *pattern++)) return 0;
		    if (!(c = *pattern++)) return 0;
		  }
		if (c1 >= toupper(cstart) && c1 <= toupper(cend)) goto match;
		if (c == ']')
		  break;
	      }
	    if (!invert) return 0;
	    break;

	  match:
	    if (invert) return 0;
	    if (c == ']')
	      break;
	    while (*pattern != ']')
	      {
	         if (*pattern == '\\')
	           ++pattern;
	         if (!*pattern) return 0;
	         ++pattern;
	      }
	    ++pattern;
	    break;
	  }

	default:
	  if (c != toupper(*text)) return 0;
	  ++text;
	  break;
	}
    }

  if (*text) return 0;
  return 1;
}


/**********************************************************************
 *
 *	glob_match_after_star
 *
 * Like glob_match, but returns 1 if any final segment of text matches pattern.
 */

static int
glob_match_after_star (pattern, text)
     register char *pattern, *text;
{
  register char c, c1;

#if 0
  printf("glob_match_after_star: pattern = '%s'\n text = '%s'\n",pattern,text);
#endif

  while ((c = toupper(*pattern)) == '?' || c == '*')
    {
      ++pattern;
      if (c == '?' && *text++ == 0)
	return 0;
    }

  ++pattern;
  if (! c)
    return 1;

  if (c == '\\') c1 = toupper(*pattern);
  else c1 = c;

  for (;;)
    {
      if ((c == '[' || toupper(*text) == c1) 
          && glob_match (pattern - 1, text))
	return 1;
      if (! *text++) return 0;
    }
}

#if 0


/**********************************************************************
 *
 *	glob_vector
 *
 * Return a vector of names of files in directory DIR
 * whose names match glob pattern PAT.
 * The names are not in any particular order.
 *
 * The vector is terminated by an element that is a null pointer.
 *
 * To free the space allocated, first free the vector's elements,
 * then free the vector.
 *
 * Return 0 if cannot get enough memory to hold the pointer
 * and the names.
 *
 * Return -1 if cannot access directory DIR.
 * Look in errno for more information.
 */

char **
glob_vector (pat, dir)
     char *pat;
     char *dir;
{
  struct globval
    {
      struct globval *next;
      char *name;
    };

  DIR *d;
  register struct direct *dp;
  struct globval *lastlink;
  register struct globval *nextlink;
  register char *nextname;
  int count;
  int lose;
  register char **vector;
  register int i;

  if (!(d = opendir (dir)))
    return (char **) -1;

  lastlink = 0;
  count = 0;
  lose = 0;

  /* Scan the directory, finding all names that match.
     For each name that matches, allocate a struct globval
     on the stack and store the name in it.
     Chain those structs together; lastlink is the front of the chain.  */
  /* Loop reading blocks */
  while (1)
    {
      dp = readdir (d);
      if (!dp) break;
      if (dp->d_ino && glob_match (pat, dp->d_name))
	{
	  nextlink = (struct globval *) alloca (sizeof (struct globval));
	  nextlink->next = lastlink;
	  nextname = (char *) malloc (dp->d_namlen + 1);
	  if (!nextname)
	    {
	      lose = 1;
	      break;
	    }
	  lastlink = nextlink;
	  nextlink->name = nextname;
	  bcopy (dp->d_name, nextname, dp->d_namlen + 1);
	  count++;
	}
    }
  closedir (d);

  vector = (char **) malloc ((count + 1) * sizeof (char *));

  /* Have we run out of memory?  */
  if (!vector || lose)
    {
      /* Here free the strings we have got */
      while (lastlink)
	{
	  free (lastlink->name);
	  lastlink = lastlink->next;
	}
      return 0;
    }

  /* Copy the name pointers from the linked list into the vector */
  for (i = 0; i < count; i++)
    {
      vector[i] = lastlink->name;
      lastlink = lastlink->next;
    }

  vector[count] = 0;
  return vector;
}

#endif /* 0 */


#if 0

main (argc, argv)
     int argc;
     char **argv;
{
  char **value = glob_vector (argv[1], argv[2]);
  int i;

  if ((int) value == 0)
    printf ("Memory exhausted.\n");
  else if ((int) value == -1)
    perror (argv[2]);
  else
    for (i = 0; value[i]; i++)
      printf ("%s\n", value[i]);

  return 0;
}

#endif /* TEST */
