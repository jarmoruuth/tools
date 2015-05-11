/************************************************************************
 *	ultoa
 *
 * Even if the ultoa() were already defined in the library, redefinition
 * does not matter, because this version is excatly compatible with
 * it and this is also well fast enough.
 *
 * Written by Petri Soini.
 */
char*	ultoa(ulong n, char* s, int radix)
{
	static char	itoc_xlate[] = {
		'0','1','2','3','4','5','6','7','8','9','A','B',
		'C','D','E','F','G','H','I','J','K','L','M','N',
		'O','P','Q','R','S','T','U','V','W','X','Y','Z'
	};
	register char*	s1;
	register char*	s2;
	
	s1 = s2 = s;
	if ((unsigned)(radix - 2) <= (unsigned)(sizeof(itoc_xlate) - 2))
		do {
			*s1++ = itoc_xlate[n % (ulong)radix];
		} while (n /= (unsigned)radix);
	for (*s1 = '\0'; --s1 > s2; ++s2)
		*s1 ^= *s2 ^= *s1 ^= *s2;	/* what on earth ??? */
	return (s);
}
