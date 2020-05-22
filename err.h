#ifndef _ERR_
#define _ERR_

/* Wypisuje informację o błędnym zakończeniu funkcji systemowej 
   i kończy działanie. */
extern void syserr(const char *fmt, ...);

/* Wypisuje informację o błędzie i kończy działanie. */
extern void fatal(const char *fmt, ...);

#endif
