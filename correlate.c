/* correlate.c
 * Originally written by Daniel Foote.
 *
 * The functions in this file match the timestamps on
 * the photos to the GPS data, and then, if a match
 * is found, writes the GPS data into the EXIF data
 * in the photo. For future reference... */

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
#include <math.h>

#include "gpsstructure.h"
#include "exif-gps.h"
#include "correlate.h"
#include "unixtime.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

/* Internal functions used to make it work. */
static void Round(const struct GPSPoint* First, struct GPSPoint* Result,
	   struct timespec PhotoTime, int HeadingOffset, int MaxHeadingDelta);
static void Interpolate(const struct GPSPoint* First, struct GPSPoint* Result,
	   struct timespec PhotoTime, int HeadingOffset, int MaxHeadingDelta);
static void Exact(const struct GPSPoint* First, struct GPSPoint* Result,
	   int HeadingOffset);

/* Ensure a heading is between 0..360 */
static double CanonicalHeading(double Heading)
{
	Heading = remainder(Heading, 360.);
	if (Heading < 0)
	{
		Heading += 360.;
	}
	return Heading;
}

/* Set the time zone parameters automatically based on this date. */
void SetAutoTimeZoneOptions(const char *Time,
		struct CorrelateOptions* Options)
{
	time_t RealTime;

	/* PhotoTime isn't a true epoch time, but is rather out
	 * by the local offset from UTC */
	struct timespec PhotoTime =
		ConvertToUnixTime(Time, EXIF_DATE_FORMAT, 0);

	/* Extract the component time values */
	struct tm *PhotoTm = gmtime(&PhotoTime.tv_sec);

	/* Then create a true epoch-based local time, including DST */
	PhotoTm->tm_isdst = -1;
	RealTime = mktime(PhotoTm);

	/* Finally, RealTime is the proper Epoch time of the photo.
	 * The difference from PhotoTime is the time zone offset. */
	Options->TimeZoneHours = (PhotoTime.tv_sec - RealTime) / 3600;
	Options->TimeZoneMins = ((PhotoTime.tv_sec - RealTime) % 3600) / 60;
}

/* Convert a time into Unixtime with the configured time zone conversion. */
struct timespec ConvertTimeToUnixTime(const char *Time, const char *TimeFormat,
		long OffsetTime, long *UsedOffset, const struct CorrelateOptions* Options)
{
	long TZOffset;
	if (Options->TimeZoneFromPhoto && OffsetTime != NO_OFFSET_TIME)
		/* Use the photo's time zone */
		TZOffset = OffsetTime;
	else
		/* Use the automatic or manually-specified time zone */
		TZOffset = Options->TimeZoneHours * 3600 + Options->TimeZoneMins * 60;
	if (UsedOffset)
		*UsedOffset = TZOffset;
	struct timespec PhotoTime = ConvertToUnixTime(Time, TimeFormat, TZOffset);

	/* Add the PhotoOffset time. This is to make the Photo time match
	 * the GPS time - i.e., it is (GPS - Photo). */
	double IntPart;
	double FracPart = modf(Options->PhotoOffset, &IntPart);
	PhotoTime.tv_sec += (long)IntPart;
	long NewNs = PhotoTime.tv_nsec + (long)round(FracPart * 1e9);
	/* Handle over/underflow out of the nanoseconds part which must be
	 * 0 <= nsec <= 999999999 */
	if (NewNs < 0) {
		PhotoTime.tv_sec -= 1;
		NewNs += 1000000000L;
	} else if (NewNs >= 1000000000L) {
		PhotoTime.tv_sec += 1;
		NewNs -= 1000000000L;
	}
	PhotoTime.tv_nsec = NewNs;
	return PhotoTime;
}

/* This function returns a GPSPoint with the point selected for the
 * file. This allows us to do funky stuff like not actually write
 * the files - ie, just correlate and keep into memory... */

struct GPSPoint* CorrelatePhoto(const char* Filename,
		long *UsedOffset, struct CorrelateOptions* Options)
{
	*UsedOffset = NO_OFFSET_TIME;
	/* Read out the timestamp from the EXIF data. */
	int IncludesGPS = 0;
	long OffsetTime = 0;
	char* TimeTemp = ReadExifData(Filename, &IncludesGPS, NULL, NULL, NULL,
			&OffsetTime);
	if (!TimeTemp)
	{
		/* Error reading the time from the file. Abort. */
		/* If this was a read error, then a seperate message
		 * will appear on the console. Otherwise, we were
		 * returned here due to the lack of exif tags. */
		Options->Result = CORR_NOEXIFINPUT;
		return NULL;
	}
	if (IncludesGPS && !Options->OverwriteExisting)
	{
		/* Already have GPS data in the file!
		 * So we can't do this again... */
		Options->Result = CORR_GPSDATAEXISTS;
		free(TimeTemp);
		return NULL;
	}
	if (Options->AutoTimeZone)
	{
		/* Use the local time zone as of the date of first picture
		 * as the time for correlating all the remainder. */
		SetAutoTimeZoneOptions(TimeTemp, Options);
		Options->AutoTimeZone = 0;
	}
	//printf("Using offset %ld sec.\n", OffsetTime);

	/* Now convert the time into Unixtime with the configured time zone conversion. */
	struct timespec PhotoTime = ConvertTimeToUnixTime(TimeTemp, EXIF_DATE_FORMAT, OffsetTime,
			UsedOffset, Options);

	/* Free the memory for the time string - it won't otherwise
	 * be freed for us. */
	free(TimeTemp);

	/* Search the list of GPS tracks to find one containing the range
	 * we're interested in. Options points to an array with the last
	 * entry denoted by a NULL Points pointer. */
	int TrackNum;
	for (TrackNum = 0; Options->Track[TrackNum].Points; ++TrackNum)
	{
		/* Check that the photo is within the times that
		 * our tracks are for. Can't really match it if
		 * we were not logging when it was taken.
		 * Note: photos taken between logging sessions of the
		 * same file will still make it inside of this. In
		 * some cases, it won't matter, but if it does, then
		 * keep this in mind!! */
		if (CompareTimes(PhotoTime, Options->Track[TrackNum].MinTime) >= 0 &&
		    CompareTimes(PhotoTime, Options->Track[TrackNum].MaxTime) <= 0)
			break;
	}
	Options->Result = CORR_NOMATCH; /* For convenience later */
	if (!Options->Track[TrackNum].Points) {
		/* All tracks were outside the time range. Abort. */
		return NULL;
	}

	/* Time to run through the list, and see if our PhotoTime
	 * is in between two points. Alternately, it might be
	 * exactly on a point... even better... */
	const struct GPSPoint* Search;
	struct GPSPoint* Actual = (struct GPSPoint*) malloc(sizeof(struct GPSPoint));
	if (!Actual) {
		Options->Result = CORR_EXIFWRITEFAIL;
		return NULL;
	}

	for (Search = Options->Track[TrackNum].Points; Search; Search = Search->Next)
	{
		/* First test: is it exactly this point? */
		if (CompareTimes(PhotoTime, Search->Time) == 0)
		{
			/* This is the point, exactly.
			 * Copy out the data and return that. */
			Exact(Search, Actual, Options->HeadingOffset);
			if (!Options->WriteHeading)
			{
				Actual->MoveHeading = -1;  // disabled
			}
			Options->Result = CORR_OK;
			break;
		}

		/* Sanity check / track segment fix: is the photo time before
		 * the current point? If so, we've gone past it. Hrm. */
		if (CompareTimes(PhotoTime, Search->Time) < 0)
		{
			Options->Result = CORR_NOMATCH;
			break;
		}

		/* Sanity check: we need to peek at the next point.
		 * Make sure we can. */
		if (Search->Next == NULL) break;
		/* Sanity check: does this point have the same
		 * timestamp as the next? If so, skip onward. */
		if (Search->Time == Search->Next->Time) continue;
		/* Sanity check: does this point have a later
		 * timestamp than the next point? If so, skip. */
		if (Search->Time > Search->Next->Time) continue;

		if (Options->DoBetweenTrkSeg)
		{
			/* Righto, we are interpolating between segments.
			 * So simply do nothing! Simple! */
		} else {
			/* Don't check between track segments.
			 * If the end of segment marker is set, then simply
			 * "jump" over this point. */
			if (Search->EndOfSegment)
			{
				continue;
			}
		}

		/* Sort of sanity check: is this photo inside our
		 * "feather" time? If not, abort. */
		if (Options->FeatherTime)
		{
			/* Is the point between these two? */
			if (CompareTimes(PhotoTime, Search->Time) > 0 &&
				CompareTimes(PhotoTime, Search->Next->Time) < 0)
			{
				/* It is. Now is it too far
				 * from these two? */
				if (CompareTimes(PhotoTime, (Search->Time + Options->FeatherTime)) > 0 &&
					CompareTimes(PhotoTime, (Search->Next->Time - Options->FeatherTime)) < 0)
				{
					/* We are inside the feather
					 * time between two points.
					 * Abort. */
					Options->Result = CORR_TOOFAR;
					free(Actual);
					return NULL;
				}
			}
		} /* endif (Options->FeatherTime) */

		/* Second test: is it between this and the
		 * next point? */
		if (CompareTimes(PhotoTime, Search->Time) > 0 &&
			CompareTimes(PhotoTime, Search->Next->Time) < 0)
		{
			/* It is between these points.
			 * Unless told otherwise, we interpolate.
			 * If not interpolating, we round to nearest.
			 * If points are equidistant, we round up. */
			if (Options->NoInterpolate)
			{
				/* No interpolation. Round. */
				Round(Search, Actual, PhotoTime,
						Options->HeadingOffset, Options->MaxHeadingDelta);
				if (!Options->WriteHeading)
				{
					Actual->MoveHeading = -1;  // disabled
				}
				Options->Result = CORR_ROUND;
				break;
			} else {
				/* Interpolate away! */
				Interpolate(Search, Actual, PhotoTime,
						Options->HeadingOffset, Options->MaxHeadingDelta);
				if (!Options->WriteHeading)
				{
					Actual->MoveHeading = -1;  // disabled
				}
				Options->Result = CORR_INTERPOLATED;
				break;
			}
		}
	} /* End for() loop to search. */

	/* Did we actually match it at all? */
	if (Options->Result == CORR_NOMATCH)
	{
		/* Nope, no match at all. */
		/* Return with nothing. */
		free(Actual);
		return NULL;
	}

	/* Write the data back into the Exif info. If we're allowed. */
	if (Options->NoWriteExif)
	{
		/* Don't write exif tags. Just return. */
		return Actual;
	} else {
		/* Do write the exif tags. And then return. */
		if (WriteGPSData(Filename, Actual, Options->Datum, Options->NoChangeMtime, Options->DegMinSecs))
		{
			/* All ok. Good! Return. */
			return Actual;
		} else {
			/* Not good. Return point, but note failure. */
			Options->Result = CORR_EXIFWRITEFAIL;
			return Actual;
		}
	}

	/* Looks like nothing matched. Free the prepared memory,
	 * and return nothing. */
	free(Actual);
	return NULL;
}

void Round(const struct GPSPoint* First, struct GPSPoint* Result,
	   struct timespec PhotoTime, int HeadingOffset, int MaxHeadingDelta)
{
	/* Round the point between the two points - ie, it will end
	 * up being one or the other point. */
	const struct GPSPoint* CopyFrom = NULL;

	/* Determine the difference between the two points.
	 * We're using the scale function used by interpolate.
	 * This gives us a good view of where we are... */
	double Scale = (double)First->Next->Time - (double)First->Time;
	Scale = ((double)PhotoTime.tv_sec + (double)PhotoTime.tv_nsec / 1e9 - (double)First->Time)
			/ Scale;

	/* Compare our scale. */
	if (Scale < 0.5)
	{
		/* Closer to the first point. */
		CopyFrom = First;
	} else {
		/* Closer to the second point. */
		CopyFrom = First->Next;
	}

	/* Copy the numbers over... */
	Result->Lat = CopyFrom->Lat;
	Result->LatDecimals = CopyFrom->LatDecimals;
	Result->Long = CopyFrom->Long;
	Result->LongDecimals = CopyFrom->LongDecimals;
	Result->Elev = CopyFrom->Elev;
	Result->ElevDecimals = CopyFrom->ElevDecimals;
	Result->HeadingRef = CopyFrom->HeadingRef;
	Result->MoveHeading = -1;
	Result->Heading = -1;

	// Drop this point if too large a change in direction
	double DeltaHeading = First->Next->MoveHeading - First->MoveHeading;
	if (MaxHeadingDelta < 0 || fabs(DeltaHeading) <= MaxHeadingDelta)
	{
		Result->MoveHeading = CopyFrom->MoveHeading;
		if (HeadingOffset >= 0 && CopyFrom->MoveHeading >= 0)
		{
			Result->Heading = CanonicalHeading(CopyFrom->MoveHeading + HeadingOffset);
		}
	}
	Result->Time = CopyFrom->Time;

	/* Done! */

}

void Interpolate(const struct GPSPoint* First, struct GPSPoint* Result,
		 struct timespec PhotoTime, int HeadingOffset, int MaxHeadingDelta)
{
	/* Interpolate between the two points. The first point
	 * is First, the other First->Next. Results into Result. */

	/* Calculate the "scale": a decimal giving the relative distance
	 * in time between the two points. Ie, a number between 0 and 1 -
	 * 0 is the first point, 1 is the next point, and 0.5 would be
	 * half way. */
	double Scale = (double)First->Next->Time - (double)First->Time;
	Scale = ((double)PhotoTime.tv_sec  + (double)PhotoTime.tv_nsec / 1e9 - (double)First->Time)
			/ Scale;

	/* Now calculate the Latitude. */
	Result->Lat = First->Lat + ((First->Next->Lat - First->Lat) * Scale);
	Result->LatDecimals = MIN(First->LatDecimals, First->Next->LatDecimals);

	/* And the longitude. */
	Result->Long = First->Long + ((First->Next->Long - First->Long) * Scale);
	Result->LongDecimals = MIN(First->LongDecimals, First->Next->LongDecimals);

	/* And the elevation. If elevation wasn't set, it should be zero with
	 * a negative ElevDecimals, which will cause it to be dropped
	 * when written. */
	Result->Elev = First->Elev + ((First->Next->Elev - First->Elev) * Scale);
	Result->ElevDecimals = MIN(First->ElevDecimals, First->Next->ElevDecimals);

	/* Heading and Direction */
	Result->HeadingRef = First->HeadingRef;
	Result->MoveHeading = -1;
	Result->Heading = -1;
	if (First->MoveHeading >= 0 && First->Next->MoveHeading >= 0)
	{
		/* Determine the smallest angle between the two headings, regardless
		 * of which way around the compass you need to go.
		 */
		double DeltaHeading1 = First->Next->MoveHeading - First->MoveHeading;
		double DeltaHeading2 = (First->Next->MoveHeading > First->MoveHeading) ?
			First->Next->MoveHeading - (360 + First->MoveHeading) :
			(360 + First->Next->MoveHeading) - First->MoveHeading;
		double DeltaHeading = fabs(DeltaHeading1) < fabs(DeltaHeading2) ?
			DeltaHeading1 : DeltaHeading2;

		// Drop this point unless the change in direction is small enough
		if (MaxHeadingDelta < 0 || fabs(DeltaHeading) <= MaxHeadingDelta)
		{
			Result->MoveHeading = CanonicalHeading(
					First->MoveHeading + DeltaHeading * Scale);
			if (HeadingOffset >= 0)
			{
				Result->Heading = CanonicalHeading(Result->MoveHeading + HeadingOffset);
			}
		}
	}

	/* The time is not interpolated, but matches photo. */
	Result->Time = PhotoTime.tv_sec;

	/* And that should have fixed us... */
}

void Exact(const struct GPSPoint* First, struct GPSPoint* Result,
	   int HeadingOffset)
{
	Result->Lat = First->Lat;
	Result->LatDecimals = First->LatDecimals;
	Result->Long = First->Long;
	Result->LongDecimals = First->LongDecimals;
	Result->Elev = First->Elev;
	Result->ElevDecimals = First->ElevDecimals;
	Result->HeadingRef = First->HeadingRef;
	Result->MoveHeading = First->MoveHeading;
	if (HeadingOffset >= 0 && First->MoveHeading >= 0)
	{
		Result->Heading = CanonicalHeading(First->MoveHeading + HeadingOffset);
	} else {
		Result->Heading = -1;  // disabled or unknown
	}
	Result->Time = First->Time;
}
