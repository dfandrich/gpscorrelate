/* exif-gps.h
 * Written by Daniel Foote.
 * Started Feb 2005.
 *
 * This file contains the prototypes for the functions
 * in exif-gps.cpp.
 * These are declared extern "C" to prevent name
 * mangling, as C++ has a habit of doing.
 */

/* Copyright 2005-2016 Daniel Foote, Dan Fandrich.
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

#ifdef __cplusplus
extern "C" {
#endif
	
char* ReadExifDate(const char* File, int* IncludesGPS);
char* ReadExifData(const char* File, double* Lat, double* Long, double* Elevation, int* IncludesGPS);
char* ReadGPSTimestamp(const char* File, char* DateStamp, char* TimeStamp, int* IncludesGPS);
int WriteGPSData(const char* File, const struct GPSPoint* Point,
		 const char* Datum, int NoChangeMtime, int DegMinSecs);
int WriteFixedDatestamp(const char* File, time_t TimeStamp);
int RemoveGPSExif(const char* File, int NoChangeMtime, int NoWriteExif);

#ifdef __cplusplus
}
#endif

