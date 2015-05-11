/*************************************************************************
 *  source       * timer.c
 *  description  * Timer functions for measuring program execution times.
 *               * 
 *               * 
 *  author       * Pekka Lampio
 *  date         * 1988-08-03
 *************************************************************************/

# include <stdio.h>
# include <time.h>
# include <timer.h>

static int timer_error(char *message)
{
	fprintf(stderr, "\nTIMER ERROR : %s\n", message);
	return 0;
}

static clock_t correction = 0;

/*
	Resets timer. After call timer is zero and stopped.
*/
void timer_reset(TIMER *timer)
{
	timer->time = 0;
	timer->running = 0;
}

/*
	Resets timer and starts it.
*/
void timer_start(TIMER *timer)
{
	timer_reset(timer);
	timer_restart(timer);
}

/*
	Restarts reset or stopped timer.
*/
int timer_restart(TIMER *timer)
{
	static correction_initializedp = 0;
	TIMER t;

	if (!correction_initializedp) {
	    correction_initializedp = 1;
	    timer_reset(&t);
	    timer_start(&t);
	    timer_stop(&t);
	    correction = timer_read(&t);
	}
	if (timer->running) return timer_error("Starting timer already running");

	timer->running = 1;
	timer->time = clock() - timer->time;
	return 1;
}

/*
	Stops a running timer
*/
int timer_stop(TIMER *timer)
{
	if (!timer->running) return timer_error("Stopping a stopped timer");

	timer->time = clock() - correction - timer->time;
	if (timer->time < 0) timer->time = 0;
	timer->running = 0;
	return 1;
}

/*
	Reads running or stopped timer. Returns elapsed time in
	milliseconds
*/
long timer_read(TIMER *timer)
{
	return(timer->running ? clock() - timer->time 
	                      : timer->time );
}

/*
	Reads running or stopped timer. Returns elapsed time in
	seconds. Fractional part contains milliseconds.
*/
float timer_readf(TIMER *timer)
{
	return((float)((double)timer_read(timer)/(double)CLK_TCK));
}
