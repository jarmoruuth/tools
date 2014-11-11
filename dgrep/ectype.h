/**********************************************************************\
 *
 *		ECTYPE.H
 *
 *	Macros for character class testing and case conversion.
 *	They use new _ectype[] and _diffcase[] tables that
 *	support IBM PC extended character set and 7 bit scandinavian
 *	ASCII character set (can be selected at compile time of ectype.c). 
 *	Tables are defined in ectype.c.
 *
 *	Author : J.Ruuth 19-Jan-1988
 *
 *	Copyright (C) 1988 by J.Ruuth
\**********************************************************************/

#ifndef _ECTYPE_
#define _ECTYPE_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char _uchar;

extern	_uchar _ectype[];	/* Character type array */
extern	_uchar _diffcase[]; 	/* Character case conversion table */

/* set bit masks for the possible character types */

#define _UPPER       0x01       /* upper case letter */
#define _LOWER       0x02       /* lower case letter */
#define _DIGIT       0x04       /* digit[0-9] */
#define _SPACE       0x08       /* tab, carriage return, newline, */
                                /* vertical tab or form feed */
#define _PUNCT       0x10       /* punctuation character */
#define _CONTROL     0x20       /* control character */
#define _BLANK       0x40       /* space char */
#define _HEX         0x80       /* hexadecimal digit */

/* the character classification macro definitions */

#define isalpha(c)      ( _ectype[(_uchar)(c)] & (_UPPER|_LOWER) )
#define isupper(c)      ( _ectype[(_uchar)(c)] & _UPPER )
#define islower(c)      ( _ectype[(_uchar)(c)] & _LOWER )
#define isdigit(c)      ( _ectype[(_uchar)(c)] & _DIGIT )
#define isxdigit(c)     ( _ectype[(_uchar)(c)] & _HEX )
#define isspace(c)      ( _ectype[(_uchar)(c)] & _SPACE )
#define ispunct(c)      ( _ectype[(_uchar)(c)] & _PUNCT )
#define isalnum(c)      ( _ectype[(_uchar)(c)] & (_UPPER|_LOWER|_DIGIT) )
#define isprint(c)      ( _ectype[(_uchar)(c)] & (_BLANK|_PUNCT|_UPPER|_LOWER|_DIGIT) )
#define isgraph(c)      ( _ectype[(_uchar)(c)] & (_PUNCT|_UPPER|_LOWER|_DIGIT) )
#define iscntrl(c)      ( _ectype[(_uchar)(c)] & _CONTROL )
#define isascii(c)      ( (_uchar)(c) < 0x80 )
#define toascii(c)	((c) & 0x7f)

#define tolower(c)      ( (isupper(c)) ? (int)_diffcase[(_uchar)(c)] : (c) )
#define toupper(c)      ( (islower(c)) ? (int)_diffcase[(_uchar)(c)] : (c) )

#ifdef __cplusplus
}
#endif
#endif	/* _ECTYPE_ */
