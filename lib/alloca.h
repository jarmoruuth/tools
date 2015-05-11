/*=======================================================================
**			ALLOCA(), by P.Soini
**			--------
**	This macro works as the alloca() library function of many 
**	C-compilers. However, this macro should be used only when the
**	caller has several local variables so that stack frame will be
**	returned with a "MOV SP,BP" instruction. This could be made sure
**	also by assigning the _SP to a variable before call
**	to ALLOCA() and assigning the value back to _SP as the last
**	expression of the function.
*/

#if defined(__TURBOC__) && !defined(_ALLOCA_DEF_)

#define _ALLOCA_DEF_

#if (defined(__COMPACT__)||defined(__LARGE__)||defined(__HUGE__))

/* Define MK_FP here to avoid multiple includes to dos.h */
#define MK_FP(seg,ofs)	((void far *) \
			   (((unsigned long)(seg) << 16) | (unsigned)(ofs)))

#define	alloca(nbytes)	MK_FP(_SS,(_SP -= (nbytes)))

#else

#define	alloca(nbytes)	(void *)(_SP -= (nbytes))

#endif

#endif /*__TURBOC__*/
