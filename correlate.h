/* correlate.h
 * Written by Daniel Foote.
 * Started Feb 2005.
 *
 * This file contains the options structure and prototypes for
 * functions in correlate.c.
 */

/* Copyright 2005-2018 Daniel Foote, Dan Fandrich.
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

/* A structure of options to pass to the correlate function.
 * Not really sure if this is needed, but... */
struct CorrelateOptions {
	int NoWriteExif;
	int OverwriteExisting;
	int NoInterpolate;
	int NoChangeMtime;
	int TimeZoneHours;  /* To add to photos to make them UTC. */
	int TimeZoneMins;
	int AutoTimeZone;
	int FeatherTime;
	char* Datum;     /* Datum of the data; when writing. */
	int DoBetweenTrkSeg; /* Match between track segments. */
	int DegMinSecs;   /* Write out data as DD MM SS.SS (more accurate than in the past) */
	
	int Result;

	int PhotoOffset; /* Offset applied to Photo time. This is ADDED to PHOTO TIME
			    to make it match GPS time. In seconds. 
			    This is (GPS - Photo) */

	struct GPSTrack *Track; /* Pointer to array of tracks to use. The last
				   track must be entirely zeros. */
};

/* Return codes in order:
 * _OK - all ok. Correlated exactly.
 * _INTERPOLATED - all ok, interpolated point.
 * _ROUND - point rounded to nearest.
 * _NOMATCH - could not find a match - photo timestamp outside GPS data
 * 	(This could be due to timezone of photos not set/set wrong).
 *      Returns NULL for Point.
 * _TOOFAR - point outside "feather" time. Too far from any point.
 *      Returns NULL for Point.
 * _EXIFWRITEFAIL - unable to write EXIF tags.
 * _NOEXIFINPUT - The source file contained no EXIF tags, or not the one we wanted. Hmm.
 *      Returns NULL for Point.
 * _GPSDATAEXISTS - There is already GPS data in the photo... you probably don't want
 *      to fiddle with it.
 *      Returns NULL for Point.
 */
#define CORR_OK             1
#define CORR_INTERPOLATED   2
#define CORR_ROUND          3
#define CORR_NOMATCH        4
#define CORR_TOOFAR         5
#define CORR_EXIFWRITEFAIL  6
#define CORR_NOEXIFINPUT    7
#define CORR_GPSDATAEXISTS  8


struct GPSPoint* CorrelatePhoto(const char* Filename, 
		struct CorrelateOptions* Options);
