/**********************************************************************\
 *
 *	COVINI.H
 *
 * Header for C language cover analyzer. This header is included in
 * every covered C file.
 *
 * Copyright (C) 1990-1991 by Jarmo Ruuth
 * May be freely copied for any non-commercial usage.
\**********************************************************************/

/* If execution count of every cover point is needed, _COVER_COUNT_
   must be defined. Covcl option c defines _COVER_COUNT_ automatically.

   If execution profile is needed, _COVER_PROFILE_ must be defined.
   Covcl option p defined _COVER_PROFILE_ automatically.
*/

/* Type for the array of cover points. If the execution count of every
   cover point is not needed (_COVER_COUNT_ not defined), char type will
   reduce the memory consumption 75% from the long type.
*/
#ifdef SSMEM_TRACE
#include "ssmemtrc.h"
#endif

#ifdef _COVER_COUNT_
typedef long _cover_t;
#else
typedef char _cover_t;
#endif

#ifdef _COVER_PROFILE_
/* Structure for profiling information of each function. */
typedef struct {
        char* ct_function;
        long  ct_count;
        long  ct_clock;
        char  ct_running;
} _cover_clock_t;
#endif

/* This macro is used in every cover point. Parameter n is index to 
   the _cover_tab_ array.
*/
#if defined(_COVER_COUNT_)
#define _COVER_(n) _cover_tab_[n]++
#elif defined(SSMEM_TRACE)
#define _COVER_(n)
#else
#define _COVER_(n) _cover_tab_[n]=1
#endif

/* This macro is used in the beginning of every function in the source.
   _COVER_ macro is used immediately after this macro. Parameter f is
   function name, parameter n the function number from the beginning
   of the file. When profiling, n is the index to the _cover_clock_
   array.
*/
#if defined(_COVER_PROFILE_)
#define _COVER_INIT_FUNCTION_(f,n) if(!_cover_initp_) \
                                        _cover_init_(); \
                                   _cover_start_clock_(f,_cover_clock_+n)
#elif defined(SSMEM_TRACE)
#define _COVER_INIT_FUNCTION_(f,n) SsMemTrcEnterFunction(f)
#else
#define _COVER_INIT_FUNCTION_(f,n) if(!_cover_initp_) \
                                        _cover_init_()
#endif

/* This macro is used every point where it is possible to return from
   a function. Parameter n is the function number from the beginning
   of the file. When profiling, n is the index to the _cover_clock_
   array. Note that this macro is used before the return statement,
   so the time spent in possible functions in return statement is
   not counted.
*/
#if defined(_COVER_PROFILE_)
#define _COVER_END_FUNCTION_(f,n) _cover_stop_clock_(_cover_clock_+n)
#elif defined(SSMEM_TRACE)
#define _COVER_END_FUNCTION_(f,n)    SsMemTrcExitFunction(0)
#else
#define _COVER_END_FUNCTION_(f,n)
#endif

/* Declarations for static variables and function */
static int	 _cover_initp_ = 0; /* Is this file already initialized? */
static _cover_t* _cover_tab_;   /* Array of cover points, allocated in 
                                   _cover_init_ */
static void _cover_init_(void); /* Module initialization function */
void _cover_init_file_(char*,char*,char*,void*,int,int);

#ifdef _COVER_PROFILE_
static _cover_clock_t* _cover_clock_;   /* Array of profile clocks for
                                           functions */
void _cover_start_clock_(char* function, _cover_clock_t* ct);
void _cover_stop_clock_(_cover_clock_t* ct);
void _cover_init_clock_(char*,char*,void*,int,int);
#endif /* _COVER_PROFILE_ */

void _cover_assert_(int not_exp, char* file, int line);
