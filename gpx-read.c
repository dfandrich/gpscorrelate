/* gpx-read.c
 * Originally written by Daniel Foote.
 *
 * This file contains routines to read the XML GPX files,
 * containing GPS data.
 */

/* Copyright 2005-2025 Daniel Foote, Dan Fandrich.
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
#include <stdlib.h>
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

/* Check whether the node is in a namespace that matches the given namespace prefix */
static int MatchXmlnsPrefix(xmlNodePtr Node, const char *Prefix)
{
	size_t Len = strlen(Prefix);
	for (xmlNsPtr Ns = Node->ns; Ns; Ns = Ns->next)
	{
		if (strncmp((const char *)Ns->href, Prefix, Len) == 0)
			return 1;  // In the given namespace
	}
	return 0;  // Not in the given namespace
}

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
	const char* HDOP;
	const char* Time;
	const char* Heading;
	char HeadingRef = 'T';

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
			HDOP = NULL;
			Time = NULL;
			Heading = NULL;

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
				} else if (strcmp((const char *)Properties->name, "lon") == 0)
				{
					Long = (const char *)Properties->children->content;
				}
			}

			/* Now, grab the elevation, time and heading.
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
				else if (strcmp((const char *)CCurrent->name, "hdop") == 0)
				{
					if (CCurrent->children)
						HDOP = (const char *)CCurrent->children->content;
				}
				else if (strcmp((const char *)CCurrent->name, "time") == 0)
				{
					if (CCurrent->children)
						Time = (const char *)CCurrent->children->content;
				}
				else if (strcmp((const char *)CCurrent->name, "extensions") == 0 && !Heading)
				{
					/* Look for various extensions holding the heading */
					for (xmlNodePtr Extensions = CCurrent->children; Extensions; Extensions = Extensions->next)
					{
						if ((Extensions->type == XML_ELEMENT_NODE) &&
							(strcmp((const char *)Extensions->name, "compass") == 0))
						{
							/* compass extension, written by OSMTracker */
							if (Extensions->children)
							{
								Heading = (const char *)Extensions->children->content;
								HeadingRef = 'M';  /* This comes from the compass */
								break;
							}

						} else if ((Extensions->type == XML_ELEMENT_NODE) &&
							(strcmp((const char *)Extensions->name, "TrackPointExtension") == 0) &&
							MatchXmlnsPrefix(Extensions, "http://www.garmin.com/xmlschemas/TrackPointExtension/"))
						{
							/* Garmin course extension */
							for (xmlNodePtr TPExt = Extensions->children; TPExt; TPExt = TPExt->next)
							{
								if ((TPExt->type == XML_ELEMENT_NODE) &&
									(strcmp((const char *)TPExt->name, "course") == 0))
								{
									if (TPExt->children)
									{
										/* This is defined as relative to true north, so the default
										 * HeadingRef is correct */
										Heading = (const char *)TPExt->children->content;
									}
								}
							}
						}
					}
				}
				else if (strcmp((const char *)CCurrent->name, "course") == 0 && !Heading)
				{
					/* course is part of the GPX 1.0 specification but not 1.1.
					 * Technically, we want the heading, not the course, but if
					 * that's all we have to work with, we'll have to use it.
					 * In land vehicles, it's the same, anyway. It is relative to
					 * true north, so the default HeadingRef is correct. */
					if (CCurrent->children)
						Heading = (const char *)CCurrent->children->content;
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
				LastPoint->Next = NewGPSPoint();
				LastPoint = LastPoint->Next;
				LastPoint->Next = NULL;
			} else {
				/* This is the first one. */
				FirstPoint = NewGPSPoint();
				FirstPoint->Next = NULL;
				LastPoint = FirstPoint;
			}
			if (LastPoint == NULL) {
				fprintf(stderr, _("Out of memory.\n"));
				abort();
			}

			/* Write the data into LastPoint, which should be a new point. */
			LastPoint->Lat = atof(Lat);
			LastPoint->LatDecimals = NumDecimals(Lat);
			LastPoint->Long = atof(Long);
			LastPoint->LongDecimals = NumDecimals(Long);
			if (Elev) {
				LastPoint->Elev = atof(Elev);
				LastPoint->ElevDecimals = NumDecimals(Elev);
			}
			if (HDOP) {
				LastPoint->HDOP = atof(HDOP);
			}
			if (Heading) {
				LastPoint->MoveHeading = atof(Heading);
				LastPoint->HeadingRef = HeadingRef;
			}
			struct timespec Timespec = ConvertToUnixTime(Time, GPX_DATE_FORMAT, 0);
			LastPoint->Time = Timespec.tv_sec;

			/* Debug...
			printf("TrackPoint. Lat %s (%f), Long %s (%f). Elev %s (%f), Time %d.\n",
					Lat, atof(Lat), Long, atof(Long), Elev, atof(Elev),
					ConvertToUnixTime(Time, GPX_DATE_FORMAT, 0));
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
	long LastTime = 0;
	int Warned = 0;
	for (Fill = Track->Points; Fill; Fill = Fill->Next)
	{
		/* Check the Min time */
		if (Fill->Time < Track->MinTime)
			Track->MinTime = Fill->Time;
		/* Check the Max time */
		if (Fill->Time > Track->MaxTime)
			Track->MaxTime = Fill->Time;
		if (Fill->Time < LastTime && !Warned) {
			struct tm *Tm = gmtime(&Fill->Time);
			char TimeStr[21];
			strftime(TimeStr, sizeof(TimeStr), "%Y-%m-%dT%H:%M:%SZ", Tm);
			fprintf(stderr,
					_("Warning: track points are not ordered by time (first bad point at %s).\n"),
					TimeStr);
			Warned = 1;
		}
		LastTime = Fill->Time;
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
