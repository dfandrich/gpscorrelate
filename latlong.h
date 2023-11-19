#include <math.h>

struct GPSPoint *NewGPSPoint(void);
int ParseLatLong(const char *latlongstr, struct GPSPoint* point);
int MakeTrackFromLatLong(const struct GPSPoint* latlong, int direction, struct GPSTrack* track);
int NumDecimals(const char *Decimal);

