/*************************************************************************
 *  source       * timer.h
 *  description  * Timer data structure.
 *               * 
 *               * 
 *  author       * Pekka Lampio 
 *  date         * 1988-08-03
 *************************************************************************/

typedef struct timerstruct {
	int running;
	long time;
} TIMER;


/* prototypes */
void timer_start(struct timerstruct *timer);
int timer_stop(struct timerstruct *timer);
void timer_reset(struct timerstruct *timer);
int timer_restart(struct timerstruct *timer);
long timer_read(struct timerstruct *timer);
float timer_readf(struct timerstruct *timer);
