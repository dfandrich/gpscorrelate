/* gpx-read.c
 * Written by Daniel Foote.
 * Started Feb 2005.
 *
 * This file contains routines to read the XML GPX files,
 * containing GPS data.
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

#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "i18n.h"
#include "gpx-read.h"
#include "unixtime.h"
#include "gpsstructure.h"
#include "latlong.h"

/* Pointers to the first and last points, used during parsing */
static struct GPSPoint* FirstPoint;
static struct GPSPoint* LastPoint;

static void ExtractTrackPoints(xmlNodePtr Start)
{
	/* The pointer passed to us should be the start
	 * of a heap of trkpt's. So walk though them,
	 * extracting what we need. */
	xmlNodePtr Current = NULL;
	xmlNodePtr CCurrent = NULL;
	xmlAttrPtr Properties = NULL;
	const char* Lat;
	const char* Long;
	const char* Elev;
	const char* Time;

	for (Current = Start; Current; Current = Current->next)
	{
		if ((Current->type == XML_ELEMENT_NODE) &&
			(strcmp((const char *)Current->name, "trkpt") == 0))
		{
			/* This is indeed a trackpoint. Extract! */

			/* Reset the vars... so we don't get
			 * the data from last run. */
			Lat = NULL;
			Long = NULL;
			Elev = NULL;
			Time = NULL;
			
			/* To get the Lat and Long, we have to
			 * extract the properties... another 
			 * linked list to walk. */
			for (Properties = Current->properties;
					Properties;
					Properties = Properties->next)
			{
				if (strcmp((const char *)Properties->name, "lat") == 0)
				{
					Lat = (const char *)Properties->children->content;
				}
				if (strcmp((const char *)Properties->name, "lon") == 0)
				{
					Long = (const char *)Properties->children->content;
				}
			}

			/* Now, grab the elevation and time.
			 * These are children of trkpt. */
			/* Oh, and what's the deal with the
			 * Node->children->content thing? */
			for (CCurrent = Current->children;
					CCurrent;
					CCurrent = CCurrent->next)
			{
				if (strcmp((const char *)CCurrent->name, "ele") == 0)
				{
					if (CCurrent->children)
						Elev = (const char *)CCurrent->children->content;
				}
				if (strcmp((const char *)CCurrent->name, "time") == 0)
				{
					if (CCurrent->children)
						Time = (const char *)CCurrent->children->content;
				}
			}

			/* Check that we have all the data. If we're missing something,
			 * then skip this point... NOTE: Elev is not required. */
			if (Time == NULL || Long == NULL || Lat == NULL)
			{
				/* Missing some data. */
				/* TODO: Really should report this upstream... */
				continue;
			}

			/* Right, now we theoretically have all the data.
			 * Allocate ourselves some memory and go for it... */
			if (FirstPoint)
			{
				/* Ok, adding to the list... */
				LastPoint->Next = (struct GPSPoint*) malloc(sizeof(struct GPSPoint));
				LastPoint = LastPoint->Next;
				LastPoint->Next = NULL;
			} else {
				/* This is the first one. */
				FirstPoint = (struct GPSPoint*) malloc(sizeof(struct GPSPoint));
				FirstPoint->Next = NULL;
				LastPoint = FirstPoint;
			}

			/* Clear the structure first... */
			LastPoint->Lat = 0;
			LastPoint->LatDecimals = 0;
			LastPoint->Long = 0;
			LastPoint->LongDecimals = 0;
			LastPoint->Elev = 0;
			LastPoint->ElevDecimals = -1; // default meaning no altitude was found
			LastPoint->Time = 0;
			LastPoint->EndOfSegment = 0;

			/* Write the data into LastPoint, which should be a new point. */
			LastPoint->Lat = atof(Lat);
			LastPoint->LatDecimals = NumDecimals(Lat);
			LastPoint->Long = atof(Long);
			LastPoint->LongDecimals = NumDecimals(Long);
			if (Elev) {
				LastPoint->Elev = atof(Elev);
				LastPoint->ElevDecimals = NumDecimals(Elev);
			}
			LastPoint->Time = ConvertToUnixTime(Time, GPX_DATE_FORMAT, 0, 0);
			
			/* Debug...
			printf("TrackPoint. Lat %s (%f), Long %s (%f). Elev %s (%f), Time %d.\n",
					Lat, atof(Lat), Long, atof(Long), Elev, atof(Elev),
					ConvertToUnixTime(Time, GPX_DATE_FORMAT, 0, 0));
			printf("Decimals %d %d %d\n", LastPoint->LatDecimals, LastPoint->LongDecimals, LastPoint->ElevDecimals);
			*/
			
					
		}
	} /* End For. */

	/* Return control to the recursive function... */
}

static void FindTrackSeg(xmlNodePtr Start)
{
	/* Go recursive till we find a <trgseg> tag. */
	xmlNodePtr Current = NULL;

	for (Current = Start; Current; Current = Current->next)
	{
		if ((Current->type == XML_ELEMENT_NODE) &&
			(strcmp((const char *)Current->name, "trkseg") == 0))
		{
			/* Found it... the children should
			 * all be trkpt's. */
			ExtractTrackPoints(Current->children);
			
			/* Mark the last point as being the end
			 * of a track segment. */
			if (LastPoint) LastPoint->EndOfSegment = 1;
			
		}
		
		/* And again, with children of this node. */
		FindTrackSeg(Current->children);
		
	} /* End For */

}

/* Determines and stores the min and max times from the GPS track */
static void GetTrackRange(struct GPSTrack* Track)
{
	if (Track->Points == NULL)
		return;

	/* Requires us to go through the list and keep
	 * the biggest and smallest. The list should,
	 * however, be sorted. But we do it this way anyway. */
	const struct GPSPoint* Fill = NULL;
	Track->MaxTime = 0;
	Track->MinTime = Track->Points->Time;
	for (Fill = Track->Points; Fill; Fill = Fill->Next)
	{
		/* Check the Min time */
		if (Fill->Time < Track->MinTime)
			Track->MinTime = Fill->Time;
		/* Check the Max time */
		if (Fill->Time > Track->MaxTime) 
			Track->MaxTime = Fill->Time;
	}
}


int ReadGPX(const char* File, struct GPSTrack* Track)
{
	/* Init the libxml library. Also checks version. */
	LIBXML_TEST_VERSION

	xmlDocPtr GPXData;
	
	/* Read the GPX data from file. */
	GPXData = xmlParseFile(File);
	if (GPXData == NULL)
	{
		fprintf(stderr, _("Failed to parse GPX data from %s.\n"), File);
		xmlCleanupParser();
		return 0;
	}

	/* Now grab the "root" node. */
	xmlNodePtr GPXRoot;
	GPXRoot = xmlDocGetRootElement(GPXData);

	if (GPXRoot == NULL)
	{
		fprintf(stderr, _("Invalid GPX file has no root.\n"));
		xmlFreeDoc(GPXData);
		xmlCleanupParser();
		return 0;
	}

	/* Check that this is indeed a GPX - the root node
	 * should be "gpx". */
	if (strcmp((const char *)GPXRoot->name, "gpx") == 0)
	{
		/* Ok, it is a GPX file. */
	} else {
		/* Not valid. */
		fprintf(stderr, _("Invalid GPX file.\n"));
		xmlFreeDoc(GPXData);
		xmlCleanupParser();
		return 0;
	}

	/* Now comes the messy part... walking the tree to find
	 * what we want.
	 * I've chosen to do it with two functions, one of which
	 * is recursive, rather than a clever inside-this-function
	 * walk the tree thing.
	 * 
	 * We start by calling the recursive function to look for
	 * <trkseg> tags, and then that function calls another
	 * when it has found one... this sub function then
	 * hauls out the <trkpt> tags with the actual data.
	 * Messy, convoluted, but it seems to work... */
	/* As to where to store the data? Again, its messy.
	 * We maintain two global vars, FirstPoint and LastPoint.
	 * FirstPoint points to the first GPSPoint done, and
	 * LastPoint is the last point done, used for the next
	 * point... we use this to build a singly-linked list. */
	/* (I think I'll just be grateful for the work that libxml
	 * puts in for me... imagine having to write an XML parser!
	 * Nasty.) */
	/* Before we go into this function, we also setlocale to "C".
	 * The GPX def indicates that the decimal separator should be
	 * ".", but certain locales specify otherwise. Which has caused issues.
	 * So we set the locale for this function, and then revert it.
	 */
	
	FirstPoint = NULL;
	LastPoint = NULL;
	
	char* OldLocale = setlocale(LC_NUMERIC, "C");
	
	FindTrackSeg(GPXRoot);

	setlocale(LC_NUMERIC, OldLocale);

	/* Clean up stuff for the XML library. */
	xmlFreeDoc(GPXData);
	xmlCleanupParser();

	Track->Points = FirstPoint;

	/* Find the time range for this track */
	GetTrackRange(Track);

	return 1;
}


void FreeTrack(struct GPSTrack* Track)
{
	/* Free the memory associated with the
	 * GPSPoint list... */
	struct GPSPoint* NextFree = NULL;
	struct GPSPoint* CurrentFree = Track->Points;
	while (1)
	{
		if (CurrentFree == NULL) break;
		NextFree = CurrentFree->Next;
		free(CurrentFree);
		CurrentFree = NextFree;
	}
	Track->Points = NULL;
}
