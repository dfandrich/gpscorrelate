/* exif-gps.cpp
 * Written by Daniel Foote.
 * Started Feb 2005.
 *
 * This file contains routines for reading dates
 * from exif data, and writing GPS data into the
 * appropriate photos.
 *
 * Uses the libexiv2 library.
 * From http://home.arcor.de/ahuggel/exiv2/
 */

/* Copyright 2005-2015 Daniel Foote, Dan Fandrich.
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

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>
#include <unistd.h>


#include "exiv2/image.hpp"
#include "exiv2/exif.hpp"
#include "exiv2/error.hpp"

#include "gpsstructure.h"
#include "exif-gps.h"

#ifdef DEBUG
#include "exiv2/futils.hpp"
#define DEBUGLOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUGLOG(...) /* no logging */
#endif

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

/* Debug
int main(int argc, char* argv[])
{

	printf("Starting with write...\n");

	struct GPSPoint Foo;

	Foo.Lat = -41.1234567;
	Foo.Long = 115.12345;
	Foo.Elev = 25.12345;
	Foo.Time = 123456;

	WriteGPSData(argv[1], &Foo, "WGS-84", 0);

	printf("Done write, now reading...\n");

	int GPS = 0;
	char* Ret = ReadExifDate(argv[1], &GPS);
	if (Ret)
	{
		printf("Date: %s.\n", Ret);
	} else {
		printf("Failed!\n");
	}

	if (GPS)
	{
		printf("Includes GPS data!\n");
	} else {
		printf("No GPS data!\n");
	}

};
*/

char* ReadExifDate(const char* File, int* IncludesGPS)
{
	// Open and read the file.
	Exiv2::Image::AutoPtr Image;

	try {
		Image = Exiv2::ImageFactory::open(File);
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to open file %s.\n", File);
		return NULL;
	}
	Image->readMetadata();
	if (Image.get() == NULL)
	{
		DEBUGLOG("Failed to read file %s %s.\n",
			 File, Exiv2::strError().c_str());
		return NULL;
	}

	Exiv2::ExifData &ExifRead = Image->exifData();

	// Read the tag out.
	Exiv2::Exifdatum& Tag = ExifRead["Exif.Photo.DateTimeOriginal"];

	// Check that the tag is not blank.
	std::string Value = Tag.toString();

	if (Value.length() == 0)
	{
		// No date/time stamp.
		// Not good.
		// Just return - above us will handle it.
		return NULL;
	}

	// Copy the tag and return that.
	char* Copy = strdup(Value.c_str());
	
	// Check if we have GPS tags.
	Exiv2::Exifdatum& GPSData = ExifRead["Exif.GPSInfo.GPSLatitude"];

	if (GPSData.count() < 3)
	{
		// No valid GPS data.
		*IncludesGPS = 0;
	} else {
		// Seems to include GPS data...
		*IncludesGPS = 1;
	}

	// Now return, passing a pointer to the date string.
	return Copy; // It's up to the caller to free this.
}

char* ReadExifData(const char* File, double* Lat, double* Long, double* Elev, int* IncludesGPS)
{
	// This function varies in that it reads
	// much more data than the last, specifically
	// for display purposes. For the GUI version.
	// Open and read the file.
	Exiv2::Image::AutoPtr Image;

	try {
		Image = Exiv2::ImageFactory::open(File);
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to open file %s.\n", File);
		return NULL;
	}
	Image->readMetadata();
	if (Image.get() == NULL)
	{
		DEBUGLOG("Failed to read file %s %s.\n",
			 File, Exiv2::strError().c_str());
		return NULL;
	}
	
	Exiv2::ExifData &ExifRead = Image->exifData();

	// Read the tag out.
	Exiv2::Exifdatum& Tag = ExifRead["Exif.Photo.DateTimeOriginal"];

	// Check that the tag is not blank.
	std::string Value = Tag.toString();

	if (Value.length() == 0)
	{
		// No date/time stamp.
		// Not good.
		// Just return - above us will handle it.
		return NULL;
	}

	// Copy the tag and return that.
	char* Copy = strdup(Value.c_str());
	
	// Check if we have GPS tags.
	*IncludesGPS = 0;
	Exiv2::Exifdatum GPSData = ExifRead["Exif.GPSInfo.GPSVersionID"];

	Value = GPSData.toString();

	if (Value.length() == 0)
	{
		// No GPS version tag.
		DEBUGLOG("No GPSVersionID.\n");

		// The EXIF spec says that this tag is mandatory if the GPS IFD
		// exists, but some cameras skip it anyway.
	}

	// Read it out and send it up!
	// What we are trying to do here is convert the
	// three rationals:
	//    dd/v mm/v ss/v
	// To a decimal
	//    dd.dddddd...
	// dd/v is easy: result = dd/v.
	// mm/v is harder:
	//    mm
	//    -- / 60 = result.
	//     v
	// ss/v is sorta easy.
	//     ss   
	//     -- / 3600 = result
	//      v   
	// Each part is added to the final number.
	Exiv2::URational RatNum;

	GPSData = ExifRead["Exif.GPSInfo.GPSLatitude"];
	if (GPSData.count() < 3)
		*Lat = nan("invalid");
	else {
		// This is enough to say it includes GPS data...
		*IncludesGPS = 1;

		RatNum = GPSData.toRational(0);
		*Lat = (double)RatNum.first / (double)RatNum.second;
		RatNum = GPSData.toRational(1);
		*Lat = *Lat + (((double)RatNum.first / (double)RatNum.second) / 60);
		RatNum = GPSData.toRational(2);
		*Lat = *Lat + (((double)RatNum.first / (double)RatNum.second) / 3600);

		GPSData = ExifRead["Exif.GPSInfo.GPSLatitudeRef"];
		if (strcmp(GPSData.toString().c_str(), "S") == 0)
		{
			// Negate the value - Western Hemisphere.
			*Lat = -*Lat;
		}
	}
	
	GPSData = ExifRead["Exif.GPSInfo.GPSLongitude"];
	if (GPSData.count() < 3)
		*Long = nan("invalid");
	else {
		RatNum = GPSData.toRational(0);
		*Long = (double)RatNum.first / (double)RatNum.second;
		RatNum = GPSData.toRational(1);
		*Long = *Long + (((double)RatNum.first / (double)RatNum.second) / 60);
		RatNum = GPSData.toRational(2);
		*Long = *Long + (((double)RatNum.first / (double)RatNum.second) / 3600);

		GPSData = ExifRead["Exif.GPSInfo.GPSLongitudeRef"];
		if (strcmp(GPSData.toString().c_str(), "W") == 0)
		{
			// Negate the value - Western Hemisphere.
			*Long = -*Long;
		}
	}

	// Finally, read elevation out. This one is simple.
	GPSData = ExifRead["Exif.GPSInfo.GPSAltitude"];
	if (GPSData.count() < 1)
		*Elev = nan("invalid");
	else {
		RatNum = GPSData.toRational(0);
		*Elev = (double)RatNum.first / (double)RatNum.second;

		// Is the altitude below sea level? If so, negate the value.
		GPSData = ExifRead["Exif.GPSInfo.GPSAltitudeRef"];
		if (GPSData.count() >= 1 && GPSData.toLong() == 1)
		{
				// Negate the elevation.
				*Elev = -*Elev;
		}
	}

	// Now return, passing a pointer to the date string.
	return Copy; // It's up to the caller to free this.
}

// This function is for the --fix-datestamp option.
// DateStamp and TimeStamp should be 12-char strings.
char* ReadGPSTimestamp(const char* File, char* DateStamp, char* TimeStamp, int* IncludesGPS)
{
	// This function varies in that it reads
	// much more data than the last, specifically
	// for display purposes. For the GUI version.
	// Open and read the file.
	Exiv2::Image::AutoPtr Image;

	try {
		Image = Exiv2::ImageFactory::open(File);
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to open file %s.\n", File);
		return NULL;
	}
	Image->readMetadata();
	if (Image.get() == NULL)
	{
		DEBUGLOG("Failed to read file %s %s.\n",
			 File, Exiv2::strError().c_str());
		return NULL;
	}
	
	Exiv2::ExifData &ExifRead = Image->exifData();

	// Read the tag out.
	Exiv2::Exifdatum& Tag = ExifRead["Exif.Photo.DateTimeOriginal"];

	// Check that the tag is not blank.
	std::string Value = Tag.toString();

	if (Value.length() == 0)
	{
		// No date/time stamp.
		// Not good.
		// Just return - above us will handle it.
		return NULL;
	}

	// Copy the tag and return that.
	char* Copy = strdup(Value.c_str());
	
	// Check if we have GPS tags.
	Exiv2::Exifdatum& GPSData = ExifRead["Exif.GPSInfo.GPSVersionID"];

	Value = GPSData.toString();

	if (Value.length() == 0)
	{
		// No GPS data.
		// Just return.
		*IncludesGPS = 0;
	} else {
		// Seems to include GPS data...
		*IncludesGPS = 1;

		Exiv2::URational RatNum1;
		Exiv2::URational RatNum2;
		Exiv2::URational RatNum3;

		// Read out the Time and Date stamp, for correction.
		GPSData = ExifRead["Exif.GPSInfo.GPSTimeStamp"];
		if (GPSData.count() < 3) {
			*IncludesGPS = 0;
			return Copy;
		}
		RatNum1 = GPSData.toRational(0);
		RatNum2 = GPSData.toRational(1);
		RatNum3 = GPSData.toRational(2);
		snprintf(TimeStamp, 12, "%02d:%02d:%02d",
				RatNum1.first, RatNum2.first, RatNum3.first);
		
		GPSData = ExifRead["Exif.GPSInfo.GPSDateStamp"];
		if (GPSData.count() < 3) {
			*IncludesGPS = 0;
			return Copy;
		}
		if (GPSData.typeId() == Exiv2::signedRational) {
			// bad type written by old gpscorrelate versions
			RatNum1 = GPSData.toRational(0);
			RatNum2 = GPSData.toRational(1);
			RatNum3 = GPSData.toRational(2);
			snprintf(DateStamp, 12, "%04d:%02d:%02d",
				 RatNum1.first, RatNum2.first, RatNum3.first);
		} else
			snprintf(DateStamp, 12, "%s", GPSData.toString().c_str());
	}

	return Copy;
}

static void EraseGpsTags(Exiv2::ExifData &ExifInfo)
{
	// Search through, find the keys that we want, and wipe them
	// Code below submitted by Marc Horowitz
	Exiv2::ExifData::iterator Iter;
	for (Exiv2::ExifData::iterator Iter = ExifInfo.begin();
		Iter != ExifInfo.end(); )
	{
		if (Iter->key().find("Exif.GPSInfo") == 0)
			Iter = ExifInfo.erase(Iter);
		else
			Iter++;
	}
}

/* Converts a floating point number with known significant decimal places
 * into a string representation of a rational number.
 * Number must be non-negative.
 */
static void ConvertToRational(double Number, int Decimals, char *Buf, int BufSize)
{
	// Calculate the appropriate denominator based on the number of
	// significant figures in the original data point.
	// Add one to deal with the fact that an even factor of 10 requires
	// one more digit to represent than the exponent (e.g. 10^3 = 1000
	// takes four digits to represent, not the 3 from the exponent).
	// Cap it at 10^9 to avoid overflow in the EXIF rational data type.
	double IntDecimals = ceil(log10(Number + 1.0));
	double Multiplier = pow(10, MAX(0, MIN(Decimals, 9 - IntDecimals)));
	int Int = (int)round(Number * Multiplier);
	snprintf(Buf, BufSize, "%d/%d", Int, (int)Multiplier);
}

/* Converts a floating point number with known significant decimal places
 * into a string representation of a set of latitude or longitude rational
 * numbers.
 */
static void ConvertToLatLongRational(double Number, int Decimals, char *Buf, int BufSize)
{
	int Deg, Min, Sec;
	Deg = (int)floor(fabs(Number)); // Slice off after decimal.
	Min = (int)floor((fabs(Number) - floor(fabs(Number))) * 60); // Now grab just the minutes.
	double FracPart = ((fabs(Number) - floor(fabs(Number))) * 60) - (double)Min; // Grab the fractional minute.
	// Calculate the appropriate denominator based on the number of
	// significant figures in the original data point. Splitting off the
	// minutes and integer seconds reduces the number of significant
	// figures by 3.6 (log10(60*60)), so round it down to 3 in order to
	// preserve the maximum precision.  Cap it at 7 to avoid overflow
	// in the EXIF rational data type.
	double Multiplier = pow(10, MAX(0, MIN(Decimals - 3, 7)));
	Sec = (int)round(FracPart * 60 * Multiplier); // Convert to seconds.
	snprintf(Buf, BufSize, "%d/1 %d/1 %d/%d", Deg, Min, Sec, (int)Multiplier);
	//printf("New style lat/long: %f -> %d/%d/ %d/%d\n", Number, Deg, Min, Sec, (int)Multiplier);
}

/* Converts a floating point number into a string representation of a set of
 * latitude or longitude rational numbers, using the older, not as accurate
 * style, which nobody should really be using any more.
 */
static void ConvertToOldLatLongRational(double Number, char *Buf, int BufSize)
{
	int Deg, Min;
	Deg = (int)floor(fabs(Number)); // Slice off after decimal.
	Min = (int)floor((fabs(Number) - floor(fabs(Number))) * 6000);
	snprintf(Buf, BufSize, "%d/1 %d/100 0/1", Deg, Min);
	//printf("Old style lat/long: %f -> %s\n", Number, Buf);
}

static void replace(Exiv2::ExifData &exif, Exiv2::ExifKey key, const Exiv2::Value *value)
{
	Exiv2::ExifData::iterator it = exif.findKey(key);
	if (it != exif.end())
		it->setValue(value);
	else
		exif.add(key, value);
}

int WriteGPSData(const char* File, const struct GPSPoint* Point,
		 const char* Datum, int NoChangeMtime, int DegMinSecs)
{
	// Write the GPS data to the file...

	struct stat statbuf;
	struct stat statbuf2;
	struct utimbuf utb;
	if (NoChangeMtime)
		stat(File, &statbuf);
	Exiv2::Image::AutoPtr Image;

	try {
		Image = Exiv2::ImageFactory::open(File);
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to open file %s.\n", File);
		return 0;
	}
	Image->readMetadata();
	if (Image.get() == NULL)
	{
		// It failed if we got here.
		DEBUGLOG("Failed to read file %s %s.\n",
			 File, Exiv2::strError().c_str());
		return 0;
	}
	
	Exiv2::ExifData &ExifToWrite = Image->exifData();

	// Make sure we're starting from a clean GPS IFD.
	// There might be lots of GPS tags existing here, since only the
	// presence of the GPSLatitude tag causes correlation to stop with
	// "GPS Already Present" error.
	EraseGpsTags(ExifToWrite);

	char ScratchBuf[100];

	// Do all the easy constant ones first.
	// GPSVersionID tag: standard says it should be four bytes: 02 02 00 00
	//  (and, must be present).
	Exiv2::Value::AutoPtr Value = Exiv2::Value::create(Exiv2::unsignedByte);
	Value->read("2 2 0 0");
	replace(ExifToWrite, Exiv2::ExifKey("Exif.GPSInfo.GPSVersionID"), Value.get());
	// Datum: the datum of the measured data. The default is WGS-84.
	if (*Datum)
		ExifToWrite["Exif.GPSInfo.GPSMapDatum"] = Datum;
	
	// Now start adding data.
	// ALTITUDE.
	// If no altitude was found in the GPX file, ElevDecimals will be -1
	if (Point->ElevDecimals >= 0) {
		// Altitude reference: byte "00" meaning "sea level".
		// Or "01" if the altitude value is negative.
		Value = Exiv2::Value::create(Exiv2::unsignedByte);
		if (Point->Elev >= 0)
		{
			Value->read("0");
		} else {
			Value->read("1");
		}
		replace(ExifToWrite, Exiv2::ExifKey("Exif.GPSInfo.GPSAltitudeRef"), Value.get());
		// And the actual altitude.
		Value = Exiv2::Value::create(Exiv2::unsignedRational);
		// 3 decimal points is beyond the limit of current GPS technology
		int Decimals = MIN(Point->ElevDecimals, 3);
		ConvertToRational(fabs(Point->Elev), Decimals, ScratchBuf, sizeof(ScratchBuf));

		/* printf("Altitude: %f -> %s\n", Point->Elev, ScratchBuf); */
		Value->read(ScratchBuf);
		replace(ExifToWrite, Exiv2::ExifKey("Exif.GPSInfo.GPSAltitude"), Value.get());
	}
	
	// LATITUDE
	// Latitude reference: "N" or "S".
	if (Point->Lat < 0)
	{
		// Less than Zero: ie, minus: means
		// Southern hemisphere. Where I live.
		ExifToWrite["Exif.GPSInfo.GPSLatitudeRef"] = "S";
	} else {
		// More than Zero: ie, plus: means
		// Northern hemisphere.
		ExifToWrite["Exif.GPSInfo.GPSLatitudeRef"] = "N";
	}
	// Now the actual latitude itself.
	// The original comment read:
	// This is done as three rationals.
	// I choose to do it as:
	//   dd/1 - degrees.
	//   mmmm/100 - minutes
	//   0/1 - seconds
	// Exif standard says you can do it with minutes
	// as mm/1 and then seconds as ss/1, but its
	// (slightly) more accurate to do it as
	//  mmmm/100 than to split it.
	// We also absolute the value (with fabs())
	// as the sign is encoded in LatRef.
	// Further note: original code did not translate between
	//   dd.dddddd to dd mm.mm - that's why we now multiply
	//   by 60*N - x60 to get minutes, xN to get to mmmm/N.
	//   N is 10^S where S is the number of significant decimal
	//   places.
	//
	// Rereading the EXIF standard, it's quite ok to do DD MM SS.SS
	// Which is much more accurate. This is the new default, unless otherwise
	// set.
	Value = Exiv2::Value::create(Exiv2::unsignedRational);

	if (DegMinSecs)
	{
		ConvertToLatLongRational(Point->Lat, Point->LatDecimals, ScratchBuf, sizeof(ScratchBuf));
	} else {
		ConvertToOldLatLongRational(Point->Lat, ScratchBuf, sizeof(ScratchBuf));
	}
	Value->read(ScratchBuf);
	replace(ExifToWrite, Exiv2::ExifKey("Exif.GPSInfo.GPSLatitude"), Value.get());
	
	// LONGITUDE
	// Longitude reference: "E" or "W".
	if (Point->Long < 0)
	{
		// Less than Zero: ie, minus: means
		// Western hemisphere.
		ExifToWrite["Exif.GPSInfo.GPSLongitudeRef"] = "W";
	} else {
		// More than Zero: ie, plus: means
		// Eastern hemisphere. Where I live.
		ExifToWrite["Exif.GPSInfo.GPSLongitudeRef"] = "E";
	}
	// Now the actual longitude itself, in the same way as latitude
	Value = Exiv2::Value::create(Exiv2::unsignedRational);

	if (DegMinSecs)
	{
		ConvertToLatLongRational(Point->Long, Point->LongDecimals, ScratchBuf, sizeof(ScratchBuf));
	} else {
		ConvertToOldLatLongRational(Point->Long, ScratchBuf, sizeof(ScratchBuf));
	}
	Value->read(ScratchBuf);
	replace(ExifToWrite, Exiv2::ExifKey("Exif.GPSInfo.GPSLongitude"), Value.get());

	// The timestamp.
	// Make up the timestamp...
	// The timestamp is taken as the UTC time of the photo.
	// If interpolation occurred, then this time is the time of the photo.
	struct tm TimeStamp = *gmtime(&(Point->Time));

	Value = Exiv2::Value::create(Exiv2::unsignedRational);
	snprintf(ScratchBuf, sizeof(ScratchBuf), "%d/1 %d/1 %d/1",
			TimeStamp.tm_hour, TimeStamp.tm_min,
			TimeStamp.tm_sec);
	Value->read(ScratchBuf);
	replace(ExifToWrite, Exiv2::ExifKey("Exif.GPSInfo.GPSTimeStamp"), Value.get());

	// And we should also do a datestamp.
	snprintf(ScratchBuf, sizeof(ScratchBuf), "%04d:%02d:%02d",
			TimeStamp.tm_year + 1900,
			TimeStamp.tm_mon + 1,
			TimeStamp.tm_mday);
	ExifToWrite["Exif.GPSInfo.GPSDateStamp"] = ScratchBuf;

	// Write the data to file.
	try {
		Image->writeMetadata();
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to write to file %s.\n", File);
		return 0;
	}

	if (NoChangeMtime)
	{
		stat(File, &statbuf2);
		utb.actime = statbuf2.st_atime;
		utb.modtime = statbuf.st_mtime;
		utime(File, &utb);
	}

	return 1;
	
}

int WriteFixedDatestamp(const char* File, time_t Time)
{
	// Write the GPS data to the file...

	struct stat statbuf;
	struct stat statbuf2;
	struct utimbuf utb;
	stat(File, &statbuf);

	Exiv2::Image::AutoPtr Image;

	try {
		Image = Exiv2::ImageFactory::open(File);
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to open file %s.\n", File);
		return 0;
	}
	Image->readMetadata();
	if (Image.get() == NULL)
	{
		// It failed if we got here.
		DEBUGLOG("Failed to read file %s %s.\n",
			 File, Exiv2::strError().c_str());
		return 0;
	}
	
	Exiv2::ExifData &ExifToWrite = Image->exifData();
	
	const struct tm TimeStamp = *gmtime(&Time);
	char ScratchBuf[100];

	snprintf(ScratchBuf, sizeof(ScratchBuf), "%04d:%02d:%02d",
			TimeStamp.tm_year + 1900,
			TimeStamp.tm_mon + 1,
			TimeStamp.tm_mday);
	ExifToWrite.erase(ExifToWrite.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSDateStamp")));
	ExifToWrite["Exif.GPSInfo.GPSDateStamp"] = ScratchBuf;

	Exiv2::Value::AutoPtr Value = Exiv2::Value::create(Exiv2::unsignedRational);
	snprintf(ScratchBuf, sizeof(ScratchBuf), "%d/1 %d/1 %d/1",
			TimeStamp.tm_hour, TimeStamp.tm_min,
			TimeStamp.tm_sec);
	Value->read(ScratchBuf);
	ExifToWrite.erase(ExifToWrite.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSTimeStamp")));
	ExifToWrite.add(Exiv2::ExifKey("Exif.GPSInfo.GPSTimeStamp"), Value.get());
	
	try {
		Image->writeMetadata();
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to write to file %s.\n", File);
		return 0;
	}
	
	// Reset the mtime.
	stat(File, &statbuf2);
	utb.actime = statbuf2.st_atime;
	utb.modtime = statbuf.st_mtime;
	utime(File, &utb);

	return 1;
}

int RemoveGPSExif(const char* File, int NoChangeMtime, int NoWriteExif)
{
	struct stat statbuf;
	struct stat statbuf2;
	struct utimbuf utb;
	if (NoChangeMtime)
		stat(File, &statbuf);

	// Open the file and start reading.
	Exiv2::Image::AutoPtr Image;
	
	try {
		Image = Exiv2::ImageFactory::open(File);
	} catch (Exiv2::Error& e) {
		DEBUGLOG("Failed to open file %s.\n", File);
		return 0;
	}
	Image->readMetadata();
	if (Image.get() == NULL)
	{
		// It failed if we got here.
		DEBUGLOG("Failed to read file %s %s.\n",
			 File, Exiv2::strError().c_str());
		return 0;
	}

	Exiv2::ExifData &ExifInfo = Image->exifData();

	if (ExifInfo.count() == 0)
	{
		DEBUGLOG("No EXIF tags in file %s.\n", File);
		return 0;
	}

	EraseGpsTags(ExifInfo);
	
	if (!NoWriteExif) {
		try {
			Image->writeMetadata();
		} catch (Exiv2::Error& e) {
			DEBUGLOG("Failed to write to file %s.\n", File);
			return 0;
		}
	}

	if (NoChangeMtime && !NoWriteExif)
	{
		stat(File, &statbuf2);
		utb.actime = statbuf2.st_atime;
		utb.modtime = statbuf.st_mtime;
		utime(File, &utb);
	}

	return 1;

}
