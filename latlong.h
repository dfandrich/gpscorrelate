/* latlong.h
 *
 * This file contains prototypes and other things for
 * latitude/longitude functions.
 */

/* Copyright 2016-2023 Dan Fandrich.
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

#include <math.h>

struct GPSPoint *NewGPSPoint(void);
int ParseLatLong(const char *latlongstr, struct GPSPoint* point);
int MakeTrackFromLatLong(const struct GPSPoint* latlong, int direction, struct GPSTrack* track);
int NumDecimals(const char *Decimal);

