/* unixtime.h
 * Originally written by Daniel Foote.
 *
 * This file contains prototypes and other things for
 * the Unix time functions.
 */

/* Copyright 2005-2012 Daniel Foote, Dan Fandrich.
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

#include <sys/types.h>
#include <time.h>

// The EXIF date/time format extended to subsecond resolution
#define EXIF_DATE_FORMAT "%d:%d:%d %d:%d:%d.%9[0-9]"
#define GPX_DATE_FORMAT "%d-%d-%dT%d:%d:%dZ"

struct timespec ConvertToUnixTime(const char* StringTime, const char* Format,
		int TZOffsetHours, int TZOffsetMinutes);
int CompareTimes(struct timespec a, time_t b);
