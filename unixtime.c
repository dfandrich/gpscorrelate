/* unixtime.c
 * Originally written by Daniel Foote.
 *
 * This file contains functions that converts a string
 * to a unix time, given a format string and compare times.
 */

/* Copyright 2005-2024 Daniel Foote, Dan Fandrich.
 *
 * This file is part of gpscorrelate.
 *
 * gpscorrelate is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gpscorrelate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gpscorrelate; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "unixtime.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

#ifdef _WIN32
/* Unfortunately, portable_timegm below isn't portable to Windows */
#define portable_timegm _mkgmtime

#else

/* Some systems have a version of this called timegm(), but it's not portable */
static time_t portable_timegm(struct tm *tm)
{
	static const char *tz;

        if (!tz) {
		tz = getenv("TZ");
		if (tz)
			/* Copy the string, since it's only guaranteed to be
			 * valid until the next getenv or setenv */
			tz = strdup(tz);
	}

	/* Set an empty TZ to force UTC */
	setenv("TZ", "", 1);
	tzset();
	time_t ret = mktime(tm);

	/* Restore the original TZ */
	if (tz)
	   setenv("TZ", tz, 1);
	else
	   unsetenv("TZ");
	tzset();
	return ret;
}
#endif

struct timespec ConvertToUnixTime(const char* StringTime, const char* Format,
		long TZOffset)
{
	/* Read the time using the specified format.
	 * The format and string being read from must
	 * have the most significant time on the left,
	 * and the least significant on the right:
	 * ie, Year on the left, seconds on the right. */

	/* Sanity check... */
	if (StringTime == NULL || Format == NULL)
	{
		return (struct timespec){0, 0};
	}

	/* Define and set up our structure. */
	struct tm Time;
	Time.tm_wday = 0;
	Time.tm_yday = 0;
	Time.tm_isdst = 0; // there is no DST in UTC
	char Subsecond[10] = "";

	/* Read out the time from the string using our format. */
	sscanf(StringTime, Format, &Time.tm_year, &Time.tm_mon,
			&Time.tm_mday, &Time.tm_hour,
			&Time.tm_min, &Time.tm_sec, Subsecond);

	/* Adjust the years for the mktime function to work. */
	Time.tm_year -= 1900;
	Time.tm_mon  -= 1;

	/* Calculate and return the Unix time. */
	time_t thetime = portable_timegm(&Time);

	/* Add our timezone offset to the time.
	 * Note also that we SUBTRACT this time. We want the
	 * result to be in UTC. */
	thetime -= TZOffset;

	/* Convert subseconds to nanoseconds */
	char Subsecbuf[] = "000000000";
	memcpy(Subsecbuf, Subsecond, MIN(sizeof(Subsecbuf)-1, strlen(Subsecond)));
	long Nanoseconds = atol(Subsecbuf);
	return (struct timespec){thetime, Nanoseconds};
}

/* Compare two times and return -1, 0 or 1 if a is <, == or > b, respectively */
int CompareTimes(struct timespec a, time_t b)
{
	if (a.tv_sec > b)
		return 1;
	if (a.tv_sec < b)
		return -1;
	// Seconds are equal; any nanoseconds in a means it's greater
	return !!a.tv_nsec;
}
