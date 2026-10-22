/**********************************************************************\
 *
 *	BM.H
 *
 * Boyer-Moore search. Gosper() builds the search tables for a pattern
 * into a new bm_t. The tables are only read when searching, so one
 * bm_t can be used by several threads at the same time.
 *
\**********************************************************************/

typedef struct bm_st bm_t;

bm_t* gosper(char* pat, int patlen, int iflag);
void  bm_free(bm_t* bm);
char* boyer_moore(const bm_t* bm, char* str, char* strend);

#ifdef TEST
void  show_boyer_moore_report(const bm_t* bm);
#endif
