/* latlong.c
 * Written by Daniel Fandrich.
 * Started Mar 2016.
 *
 * This file contains routines to parse latitude/longitude.
 */

/* Copyright 2016 Daniel Fandrich.
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

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gpsstructure.h"
#include "latlong.h"

/* Initialize a new GPSPoint struct */
struct GPSPoint *NewGPSPoint(void) {
	struct GPSPoint *Point = (struct GPSPoint*) malloc(sizeof(struct GPSPoint));
	if (Point) {
		Point->Lat = 0;
		Point->LatDecimals = 0;
		Point->Long = 0;
		Point->LongDecimals = 0;
		Point->Elev = 0;
		Point->ElevDecimals = -1;	// default meaning no altitude was found
		Point->MoveHeading = -1;	// default meaning no heading was found
		Point->Heading = -1;		// this is set later, if enabled
		Point->Time = 0;
		Point->EndOfSegment = 0;
		Point->Next = NULL;
	}
	return Point;
}

/* TODO: support locales that use other than "." as the decimal separator */

/* Returns the number of decimal places in the given decimal number string
   This does not support exponential notation. */
int NumDecimals(const char *Decimal)
{
	const char *Dec = strchr(Decimal, '.');
	if (Dec) {
		return strspn(Dec+1,"0123456789");
	}
	return 0;
}

/* Parses a human-readable latitude, longitude and optionally elevation in
   decimal form
   e.g. 12.3456 -123.45678 1234.56
*/
#define DEC_DELIMS " \t,"
#define DEC_NUMS "-+.0123456789eE"  // E allows exponential notation
static int ParseLatLongDecimal(const char *latlongstr, struct GPSPoint* point)
{
	char *num;
	char *str = strdup(latlongstr);
	if (!str)
		goto err;

	errno = 0; /* set by strtod */

	/* Latitude */
	num = strtok(str, DEC_DELIMS);
	if (!str || !point || !num || strlen(num) != strspn(num, DEC_NUMS))
		goto err;
	point->Lat = strtod(num, NULL);
	if (errno || point->Lat > 90 || point->Lat < -90)
		goto err;
	point->LatDecimals = NumDecimals(num);

	/* Longitude */
	num = strtok(NULL, DEC_DELIMS);
	if (!num || strlen(num) != strspn(num, DEC_NUMS))
		goto err;
	point->Long = strtod(num, NULL);
	if (errno || point->Long > 180 || point->Long < -180)
		goto err;
	point->LongDecimals = NumDecimals(num);

	/* Elevation */
	num = strtok(NULL, DEC_DELIMS);
	if (!num || !*num) {
		/* No elevation given */
		point->Elev = 0;
		point->ElevDecimals = -1; /* default meaning no altitude was found */
	} else if (strlen(num) != strspn(num, DEC_NUMS)) {
		goto err;
	} else {
		point->Elev = strtod(num, NULL);
		if (errno)
			goto err;
		point->ElevDecimals = NumDecimals(num);
	}

	point->Time = 0;
	point->EndOfSegment = 0;
	point->Next = NULL;
	free(str);
	return 1;

err:
	free(str);
	return 0;
}

/* Skip leading spaces and commas. */
static void SkipLeadingSep(char **str)
{
	while (**str == ' ' || **str == '\t' || **str == ',')
		++*str;
}

#define D_DIGITS "-+0123456789"
#define M_DIGITS "0123456789"
#define S_DIGITS ".0123456789"
/* *endstr returns the character after the last one used, or in some
 * circumstances, two after the last one used */
static double ParseDMS(char *latlongstr, char **endstr, int *dec)
{
	double dms, ms;
	char *num;
	errno = 0; /* set by strtod */

	/* Skip leading spaces and commas. Skipping commas at the beginning of
	   the latitude isn't really very good, but GIGO. */
	SkipLeadingSep(&latlongstr);

	/* Degrees
	   The degrees symbol (\xc2\xb0 in UTF-8) only makes sense in UTF-8
	   locales, but should be pretty harmless in other locales.
	   Conveniently, \xb0 is also the degrees symbol in all ISO 8859/X
	   locales that have the character, so it will work almost everywhere
	   without additional work. */
	num = strtok(latlongstr, "d\xc2\xb0");
	if (!num || strlen(num) != strspn(num, D_DIGITS))
		return NAN;
	dms = strtod(num, NULL);
	if (errno)
		return NAN;

	/* Minutes
	   The \xb0 here is a hack to allow the degrees symbol to work in the
	   previous strtok call; it's the second of the two UTF-8 bytes
	   representing that code point that will appear as the first character
	   here. Unfortunately, it appears that strtok doesn't handle UTF-8 properly,
	   even in a UTF-8 locale. */
	num = strtok(NULL, "m'\xb0 ");
	if (!num || strlen(num) != strspn(num, M_DIGITS))
		return NAN;
	ms = strtod(num, NULL);
	if (errno || ms >= 60)
		return NAN;
	dms = dms + ms/60.0 * (2*(dms>0)-1);

	/* Seconds */
	num = strtok(NULL, "s\" \t,");
	if (!num || strlen(num) != strspn(num, S_DIGITS))
		return NAN;
	ms = strtod(num, NULL);
	if (errno || ms >= 60)
		return NAN;
	dms = dms + ms/3600.0 * (2*(dms>0)-1);

	*endstr = num + strlen(num) + 1;
	/* The whole minutes and seconds add almost 4 decimals, but by the time the
	   number is converted to decimal and back, 3 works better */
	*dec = NumDecimals(num) + 3;
	return dms;
}

/* Parses a human-readable latitude, longitude and optionally elevation
   in degrees minutes seconds form
   e.g. 12d34'56.7"
*/
static int ParseLatLongDMS(const char *latlongstr, struct GPSPoint* point)
{
	char *endstr;
	/* Copy the string but add an extra NIL at the end due to ParseDMS'
	   potentially returning that character */
	char *str = (char *)calloc(strlen(latlongstr)+2, 1);
	int dec;

	if (!str)
		goto err;

	strcpy(str, latlongstr);

	/* Latitude */
	point->Lat = ParseDMS(str, &endstr, &dec);
	if (isnan(point->Lat) || point->Lat > 90 || point->Lat < -90)
		goto err;
	point->LatDecimals = dec;

	/* Longitude */
	point->Long = ParseDMS(endstr, &endstr, &dec);
	if (isnan(point->Long) || point->Long > 180 || point->Long < -180)
		goto err;
	point->LongDecimals = dec;

	/* Elevation */
	if (!*endstr) {
		/* No elevation given */
		point->Elev = 0;
		point->ElevDecimals = -1; /* default meaning no altitude was found */
	} else {
		SkipLeadingSep(&endstr);
		errno = 0; /* set by strtod */
		point->Elev = strtod(endstr, NULL);
		if (errno)
			goto err;
		point->ElevDecimals = NumDecimals(endstr);
	}
	point->Time = 0;
	point->EndOfSegment = 0;
	point->Next = NULL;
	free(str);
	return 1;

err:
	free(str);
	return 0;
}

int ParseLatLong(const char *latlongstr, struct GPSPoint* point)
{
	if (!ParseLatLongDecimal(latlongstr, point))
		if (!ParseLatLongDMS(latlongstr, point))
			return 0;
	return 1;
}

/* Make a track from a single point */
int MakeTrackFromLatLong(const struct GPSPoint* latlong, struct GPSTrack* track)
{
	struct GPSPoint* p1 = NewGPSPoint();
	struct GPSPoint* p2 = NewGPSPoint();
	if (!p1 || !p2) {
		free(p1);
		free(p2);
		return 0;
	}
	memcpy(p1, latlong, sizeof(*p1));
	p1->Time = track->MinTime = 0;
	p1->Next = p2;

	memcpy(p2, latlong, sizeof(*p2));
	p2->Time = track->MaxTime = INT_MAX;
	p2->Next = NULL;
	p2->EndOfSegment = 1;

	track->Points = p1;
	return 1;
}
