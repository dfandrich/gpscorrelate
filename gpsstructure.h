/* gpsstructure.h
 * Written by Daniel Foote.
 * Started Feb 2005.
 *
 * This file contains the structure that stores
 * GPS data points. Placed here so that several things
 * could share it.
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

/* This data structure describes a single point.
 * The NewGpsPoint() function allocates an initialized one. */

struct GPSPoint {
	double Lat;
	int LatDecimals;
	double Long;
	int LongDecimals;
	double Elev;
	int ElevDecimals;
	double MoveHeading;
	double Heading;
	char HeadingRef;	// Direction reference: byte "T" meaning true direction
						// or "M" meaning magnetic direction.
	time_t Time;
	int EndOfSegment;
	struct GPSPoint* Next;
};

struct GPSTrack {
	struct GPSPoint* Points;
	time_t MinTime;
	time_t MaxTime;
};
