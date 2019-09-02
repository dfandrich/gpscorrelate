/* gui.c
 * Written by Daniel Foote.
 * Started Feb 2005.
 *
 * The base of this file was generated with Glade, and then
 * hand edited by me for some strange reason.
 *
 * This file contains the code to create, generate, and look after
 * a GTK GUI for the photo correlation program. */

/* This is "basically" the GUI version of it. The only other
 * part of the gui program is main-gui.c, but that ended up
 * being little more than a "stub" to get the GUI up and running. */

/* Copyright 2005-2019 Daniel Foote, Dan Fandrich.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "i18n.h"
#include "gpsstructure.h"
#include "gui.h"
#include "exif-gps.h"
#include "gpx-read.h"
#include "correlate.h"

/* Declare all our widgets. Global to this module. */
GtkWidget *MatchWindow;
GtkWidget *WindowHBox;
GtkWidget *ControlsVBox;

GtkWidget *AddPhotosFrame;
GtkWidget *AddPhotosAlignment;
GtkWidget *AddPhotosVBox;
GtkWidget *PhotoAddButton;
GtkWidget *PhotoRemoveButton;
GtkWidget *AddPhotosLabel;

GtkWidget *GPSDataFrame;
GtkWidget *GPSDataAlignment;
GtkWidget *GPSDataVBox;
GtkWidget *GPSSelectedLabel;
GtkWidget *SelectGPSButton;
GtkWidget *GPSDataLabel;

GtkWidget *OptionsFrame;
GtkWidget *OptionsAlignment;
GtkWidget *OptionsVBox;
GtkWidget *InterpolateCheck;
GtkWidget *NoWriteCheck;
GtkWidget *OverwriteCheck;
GtkWidget *NoMtimeCheck;
GtkWidget *BetweenSegmentsCheck;
GtkWidget *DegMinSecsCheck;
GtkWidget *AutoTimeZoneCheck;
GtkWidget *OptionsTable;
GtkWidget *MaxGapTimeLabel;
GtkWidget *TimeZoneLabel;
GtkWidget *PhotoOffsetLabel;
GtkWidget *GPSDatumLabel;
GtkWidget *GapTimeEntry;
GtkWidget *TimeZoneEntry;
GtkWidget *PhotoOffsetEntry;
GtkWidget *GPSDatumEntry;
GtkWidget *OptionsFrameLable;

GtkWidget *CorrelateFrame;
GtkWidget *CorrelateAlignment;
GtkWidget *CorrelateButton;
GtkWidget *CorrelateLabel;

GtkWidget *OtherOptionsFrame;
GtkWidget *OtherOptionsVBox;
GtkWidget *OtherOptionsAlignment;
GtkWidget *OtherOptionsLabel;
GtkWidget *StripGPSButton;
GtkWidget *AboutButton;

GtkWidget *PhotoListVBox;
GtkWidget *PhotoListScroll;
GtkWidget *PhotoList;
#if !GTK_CHECK_VERSION(2, 12, 0)
GtkTooltips *tooltips;
#endif

/* Enum and other stuff for the Photo list box. */
enum
{
	LIST_FILENAME,
	LIST_LAT,
	LIST_LONG,
	LIST_ELEV,
	LIST_TIME,
	LIST_STATE,
	LIST_POINTER,
	LIST_NOCOLUMNS
};

GtkListStore *PhotoListStore;
GtkCellRenderer *PhotoListRenderer;
GtkTreeViewColumn *FileColumn;
GtkTreeViewColumn *LatColumn;
GtkTreeViewColumn *LongColumn;
GtkTreeViewColumn *ElevColumn;
GtkTreeViewColumn *TimeColumn;
GtkTreeViewColumn *StateColumn;

/* Structure and variables for holding the list of
 * photos in memory. */

struct GUIPhotoList {
	char* Filename;
	char* Time;
	GtkTreeIter ListPointer;
	struct GUIPhotoList* Next;
};

struct GUIPhotoList* FirstPhoto = NULL;
struct GUIPhotoList* LastPhoto = NULL;

struct GPSTrack* GPSData;  	// Array of track entries; empty entry is last
int NumTracks;			// Number of entries at GPSData

static const char* const ConfigDefaults[] = {
	"interpolate", "true",
	"dontwrite", "false",
	"nochangemtime", "false",
	"betweensegments", "false",
	"writeddmmss", "true",
	"maxgap", "0",
	"timezone", "+0:00",
	"photooffset", "0",
	"gpsdatum", "WGS-84",
	"gpxopendir", "",
	"photoopendir", "",
	NULL, NULL
};

GKeyFile* GUISettings;
char* SettingsFilename;
gchar* GPXOpenDir = NULL;
gchar* PhotoOpenDir = NULL;

static gboolean DestroyWindow(GtkWidget *Widget, GdkEvent *Event, gpointer Data);

static void AddPhotosButtonPress(GtkWidget *Widget, gpointer Data);
static void AddPhotoToList(const char* Filename);
static void RemovePhotosButtonPress( GtkWidget *Widget, gpointer Data );

static void SetListItem(GtkTreeIter* Iter, const char* Filename,
			const char* Time, double Lat, double Long, double Elev,
			const char* PassedState, int IncludesGPS);

static void SelectGPSButtonPress( GtkWidget *Widget, gpointer Data );
static void CorrelateButtonPress( GtkWidget *Widget, gpointer Data );
static void StripGPSButtonPress( GtkWidget *Widget, gpointer Data );
static void AboutButtonPress( GtkWidget *Widget, gpointer Data );

static void GtkGUIUpdate(void);

/* Load settings, insert defaults. */
void LoadSettings(void)
{
	/* Generate the filename. */
	const char* UserHomeDir = g_get_user_config_dir();
	const int FilenameLength = strlen(UserHomeDir) + 30;
	SettingsFilename = (char*) malloc(sizeof(char) * FilenameLength);

	snprintf(SettingsFilename, FilenameLength, "%s%c.gpscorrelaterc", UserHomeDir, G_DIR_SEPARATOR);

	/* Create a new key file. */
	GUISettings = g_key_file_new();

	if (!g_key_file_load_from_file(GUISettings, SettingsFilename, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		/* Unable to load the file. Oh well. */
	}

	/* Now create all the default settings. */
	int i = 0;
	while (ConfigDefaults[i])
	{
		/* If the setting doesn't exist, set the default. */
		if (NULL == g_key_file_get_value(GUISettings, "default", ConfigDefaults[i], NULL))
		{
			g_key_file_set_value(GUISettings, "default", ConfigDefaults[i], ConfigDefaults[i+1]);
		}

		i += 2;
	}

	/* Fetch the directory settings from the file. */
	PhotoOpenDir = g_key_file_get_value(GUISettings, "default", "photoopendir", NULL);
	GPXOpenDir = g_key_file_get_value(GUISettings, "default", "gpxopendir", NULL);
}

void SaveSettings(void)
{
	/* Save the settings to file, and deallocate the settings. */
	FILE* OutputFile;

	OutputFile = fopen(SettingsFilename, "w");
	if (OutputFile)
	{
		gsize SettingsLength = 0;
		gchar* SettingsString = g_key_file_to_data(GUISettings, &SettingsLength, NULL);

		fwrite((void*)SettingsString, sizeof(gchar), (size_t)SettingsLength, OutputFile);

		fclose(OutputFile);

		g_free(SettingsString);
	}
	free(SettingsFilename);
}

static void timezone_toggle_visibility(GtkWidget *widget,
                                       GtkWidget *entry )
{
  gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  gtk_widget_set_sensitive(entry, !active);
}

GtkWidget* CreateMatchWindow (void)
{
  GError *error;
  gboolean b;

  /* Load the settings. */
  LoadSettings();

#if !GTK_CHECK_VERSION(2, 12, 0)
  /* Get our tooltips ready. */
  tooltips = gtk_tooltips_new ();
#endif

  /* Start with the window itself. */
  MatchWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  char title[40];
  snprintf(title, sizeof(title), _("GPS Photo Correlate %s"), PACKAGE_VERSION);
  gtk_window_set_title (GTK_WINDOW (MatchWindow), title);
  gtk_window_set_default_size (GTK_WINDOW (MatchWindow), 792, -1);

  g_signal_connect (G_OBJECT (MatchWindow), "delete_event",
  		G_CALLBACK (DestroyWindow), NULL);

  WindowHBox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (WindowHBox);
  gtk_container_add (GTK_CONTAINER (MatchWindow), WindowHBox);

  /* The controls side of the window. */
  ControlsVBox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (ControlsVBox);
  gtk_box_pack_start (GTK_BOX (WindowHBox), ControlsVBox, FALSE, TRUE, 0);

  /* Add/remove photos area. */
  AddPhotosFrame = gtk_frame_new (NULL);
  gtk_widget_show (AddPhotosFrame);
  gtk_box_pack_start (GTK_BOX (ControlsVBox), AddPhotosFrame, FALSE, FALSE, 0);

  AddPhotosAlignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (AddPhotosAlignment);
  gtk_container_add (GTK_CONTAINER (AddPhotosFrame), AddPhotosAlignment);
  gtk_alignment_set_padding (GTK_ALIGNMENT (AddPhotosAlignment), 0, 4, 12, 4);

  AddPhotosVBox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (AddPhotosVBox);
  gtk_container_add (GTK_CONTAINER (AddPhotosAlignment), AddPhotosVBox);

  PhotoAddButton = gtk_button_new_with_mnemonic (_("Add..."));
  gtk_widget_show (PhotoAddButton);
  gtk_box_pack_start (GTK_BOX (AddPhotosVBox), PhotoAddButton, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (PhotoAddButton, _("Add photos to be correlated."));
#else
  gtk_tooltips_set_tip (tooltips, PhotoAddButton, _("Add photos to be correlated."), NULL);
#endif
  g_signal_connect (G_OBJECT (PhotoAddButton), "clicked",
  		G_CALLBACK (AddPhotosButtonPress), NULL);

  PhotoRemoveButton = gtk_button_new_with_mnemonic (_("Remove"));
  gtk_widget_show (PhotoRemoveButton);
  gtk_box_pack_start (GTK_BOX (AddPhotosVBox), PhotoRemoveButton, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (PhotoRemoveButton, _("Remove selected photos from the list."));
#else
  gtk_tooltips_set_tip (tooltips, PhotoRemoveButton, _("Remove selected photos from the list."), NULL);
#endif
  g_signal_connect (G_OBJECT (PhotoRemoveButton), "clicked",
  		G_CALLBACK (RemovePhotosButtonPress), NULL);

  AddPhotosLabel = gtk_label_new (_("<b>1. Add Photos</b>"));
  gtk_widget_show (AddPhotosLabel);
  gtk_frame_set_label_widget (GTK_FRAME (AddPhotosFrame), AddPhotosLabel);
  gtk_label_set_use_markup (GTK_LABEL (AddPhotosLabel), TRUE);

  /* GPS data area */
  GPSDataFrame = gtk_frame_new (NULL);
  gtk_widget_show (GPSDataFrame);
  gtk_box_pack_start (GTK_BOX (ControlsVBox), GPSDataFrame, FALSE, FALSE, 0);

  GPSDataAlignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (GPSDataAlignment);
  gtk_container_add (GTK_CONTAINER (GPSDataFrame), GPSDataAlignment);
  gtk_alignment_set_padding (GTK_ALIGNMENT (GPSDataAlignment), 0, 4, 12, 4);

  GPSDataVBox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (GPSDataVBox);
  gtk_container_add (GTK_CONTAINER (GPSDataAlignment), GPSDataVBox);

  GPSSelectedLabel = gtk_label_new (_("Read from: No file"));  /* FIX ME: Label not appropriately sized/placed for data. */
  gtk_widget_show (GPSSelectedLabel);
  gtk_box_pack_start (GTK_BOX (GPSDataVBox), GPSSelectedLabel, FALSE, FALSE, 0);
  gtk_label_set_ellipsize(GTK_LABEL(GPSSelectedLabel), PANGO_ELLIPSIZE_END); 
  /*gtk_label_set_width_chars(GTK_LABEL(GPSSelectedLabel), 20);
  gtk_label_set_line_wrap(GTK_LABEL(GPSSelectedLabel), TRUE);*/

  SelectGPSButton = gtk_button_new_with_mnemonic (_("Choose..."));
  gtk_widget_show (SelectGPSButton);
  gtk_box_pack_start (GTK_BOX (GPSDataVBox), SelectGPSButton, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (SelectGPSButton,
	_("Choose GPX file from which to read GPS data. If the GPS data is not in the "
	  "GPX format, use a converter like GPSBabel to convert it to GPX first."));
#else
  gtk_tooltips_set_tip (tooltips, SelectGPSButton,
	_("Choose GPX file from which to read GPS data. If the GPS data is not in the "
	  "GPX format, use a converter like GPSBabel to convert it to GPX first."), NULL);
#endif
  g_signal_connect (G_OBJECT (SelectGPSButton), "clicked",
  		G_CALLBACK (SelectGPSButtonPress), NULL);

  GPSDataLabel = gtk_label_new (_("<b>2. GPS Data</b>"));
  gtk_widget_show (GPSDataLabel);
  gtk_frame_set_label_widget (GTK_FRAME (GPSDataFrame), GPSDataLabel);
  gtk_label_set_use_markup (GTK_LABEL (GPSDataLabel), TRUE);

  /* Options area. */
  OptionsFrame = gtk_frame_new (NULL);
  gtk_widget_show (OptionsFrame);
  gtk_box_pack_start (GTK_BOX (ControlsVBox), OptionsFrame, FALSE, FALSE, 0);

  OptionsAlignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (OptionsAlignment);
  gtk_container_add (GTK_CONTAINER (OptionsFrame), OptionsAlignment);
  gtk_alignment_set_padding (GTK_ALIGNMENT (OptionsAlignment), 0, 4, 12, 4);

  OptionsVBox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (OptionsVBox);
  gtk_container_add (GTK_CONTAINER (OptionsAlignment), OptionsVBox);

  InterpolateCheck = gtk_check_button_new_with_mnemonic (_("Interpolate"));
  gtk_widget_show (InterpolateCheck);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), InterpolateCheck, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (InterpolateCheck,
	_("Interpolate between points. If disabled, points will be rounded to "
	  "the nearest recorded point."));
#else
  gtk_tooltips_set_tip (tooltips, InterpolateCheck,
	_("Interpolate between points. If disabled, points will be rounded to "
	  "the nearest recorded point."), NULL);
#endif
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (InterpolateCheck), g_key_file_get_boolean(GUISettings, "default", "interpolate", NULL));

  NoWriteCheck = gtk_check_button_new_with_mnemonic (_("Don't write"));
  gtk_widget_show (NoWriteCheck);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), NoWriteCheck, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (NoWriteCheck,
	_("Don't write EXIF data back to the photos. This is useful for "
	  "testing the settings without modifying the photos."));
#else
  gtk_tooltips_set_tip (tooltips, NoWriteCheck,
	_("Don't write EXIF data back to the photos. This is useful for "
	  "testing the settings without modifying the photos."), NULL);
#endif
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NoWriteCheck), g_key_file_get_boolean(GUISettings, "default", "dontwrite", NULL));

  OverwriteCheck = gtk_check_button_new_with_mnemonic (_("Replace existing tags"));
  gtk_widget_show (OverwriteCheck);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), OverwriteCheck, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (OverwriteCheck, _("Replace any existing GPS tags in the photos."));
#else
  gtk_tooltips_set_tip (tooltips, OverwriteCheck, _("Replace any existing GPS tags in the photos."), NULL);
#endif
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (OverwriteCheck), g_key_file_get_boolean(GUISettings, "default", "replace", NULL));

  NoMtimeCheck = gtk_check_button_new_with_mnemonic (_("Don't change mtime"));
  gtk_widget_show (NoMtimeCheck);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), NoMtimeCheck, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (NoMtimeCheck,
	_("Don't change file modification time of the photos."));
#else
  gtk_tooltips_set_tip (tooltips, NoMtimeCheck,
	_("Don't change file modification time of the photos."), NULL);
#endif
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NoMtimeCheck), g_key_file_get_boolean(GUISettings, "default", "nochangemtime", NULL));

  BetweenSegmentsCheck = gtk_check_button_new_with_mnemonic (_("Between Segments"));
  gtk_widget_show (BetweenSegmentsCheck);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), BetweenSegmentsCheck, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (BetweenSegmentsCheck,
	_("Interpolate between track segments. Generally the data is segmented "
	  "to show where location data was available and not available, but you might "
	  "still want to interpolate between segments."));
#else
  gtk_tooltips_set_tip (tooltips, BetweenSegmentsCheck,
	_("Interpolate between track segments. Generally the data is segmented "
	  "to show where location data was available and not available, but you might "
	  "still want to interpolate between segments."), NULL);
#endif
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (BetweenSegmentsCheck), g_key_file_get_boolean(GUISettings, "default", "betweensegments", NULL));

  DegMinSecsCheck = gtk_check_button_new_with_mnemonic (_("Write DD MM SS.SS"));
  gtk_widget_show (DegMinSecsCheck);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), DegMinSecsCheck, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (DegMinSecsCheck,
	_("Write the latitude and longitude values as DD MM SS.SS; this is "
	  "the new default. The old behaviour was to write it as "
	  "DD MM.MM, which will occur if you uncheck this box."));
#else
  gtk_tooltips_set_tip (tooltips, DegMinSecsCheck,
	_("Write the latitude and longitude values as DD MM SS.SS; this is "
	  "the new default. The old behaviour was to write it as "
	  "DD MM.MM, which will occur if you uncheck this box."), NULL);
#endif
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (DegMinSecsCheck), g_key_file_get_boolean(GUISettings, "default", "writeddmmss", NULL));

  AutoTimeZoneCheck = gtk_check_button_new_with_mnemonic (_("Auto time zone"));
  gtk_widget_show (AutoTimeZoneCheck);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), AutoTimeZoneCheck, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (AutoTimeZoneCheck,
    _("Assume the camera time zone is the same as the local time zone."));
#else
  gtk_tooltips_set_tip (tooltips, AutoTimeZoneCheck,
    _("Assume the camera time zone is the same as the local time zone."), NULL);
#endif
  b = g_key_file_get_boolean(GUISettings, "default", "autotimezone", &error);
  if (error && error->code)
      b = TRUE;  /* Default this to true */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (AutoTimeZoneCheck), b);

  OptionsTable = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (OptionsTable);
  gtk_box_pack_start (GTK_BOX (OptionsVBox), OptionsTable, TRUE, TRUE, 0);

  MaxGapTimeLabel = gtk_label_new (_("Max gap time:"));
  gtk_widget_show (MaxGapTimeLabel);
  gtk_table_attach (GTK_TABLE (OptionsTable), MaxGapTimeLabel, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (MaxGapTimeLabel), 0, 0.5);

  TimeZoneLabel = gtk_label_new (_("Time Zone:"));
  gtk_widget_show (TimeZoneLabel);
  gtk_table_attach (GTK_TABLE (OptionsTable), TimeZoneLabel, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (TimeZoneLabel), 0, 0.5);
  
  PhotoOffsetLabel = gtk_label_new (_("Photo Offset:"));
  gtk_widget_show (PhotoOffsetLabel);
  gtk_table_attach (GTK_TABLE (OptionsTable), PhotoOffsetLabel, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (PhotoOffsetLabel), 0, 0.5);

  GPSDatumLabel = gtk_label_new (_("GPS Datum:"));
  gtk_widget_show (GPSDatumLabel);
  gtk_table_attach (GTK_TABLE (OptionsTable), GPSDatumLabel, 0, 1, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (GPSDatumLabel), 0, 0.5);

  GapTimeEntry = gtk_entry_new ();
  gtk_widget_show (GapTimeEntry);
  gtk_table_attach (GTK_TABLE (OptionsTable), GapTimeEntry, 1, 2, 0, 1,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (GapTimeEntry,
        _("Maximum time \"away\" from a point that the photo can be taken "
          "yet still match, in seconds. If a photo's time is outside "
          "this value (from both points on either side), the location will "
	  "not match."));
#else
  gtk_tooltips_set_tip (tooltips, GapTimeEntry,
        _("Maximum time \"away\" from a point that the photo can be taken "
          "yet still match, in seconds. If a photo's time is outside "
          "this value (from both points on either side), the location will "
	  "not match."), NULL);
#endif
  gtk_entry_set_text (GTK_ENTRY (GapTimeEntry), g_key_file_get_value(GUISettings, "default", "maxgap", NULL));
  gtk_entry_set_width_chars (GTK_ENTRY (GapTimeEntry), 7);

  TimeZoneEntry = gtk_entry_new ();
  gtk_widget_show (TimeZoneEntry);
  gtk_table_attach (GTK_TABLE (OptionsTable), TimeZoneEntry, 1, 2, 1, 2,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (TimeZoneEntry,
	_("The timezone that the camera's time was set to when the photos were "
	  "taken. For example, if a camera is set to AWST or +8:00 hours from UTC, "
	  "enter +8:00 here so that the correct adjustment to the photo's time "
	  "can be made. GPS data is always in UTC."));
#else
  gtk_tooltips_set_tip (tooltips, TimeZoneEntry,
	_("The timezone that the camera's time was set to when the photos were "
	  "taken. For example, if a camera is set to AWST or +8:00 hours from UTC, "
	  "enter +8:00 here so that the correct adjustment to the photo's time "
	  "can be made. GPS data is always in UTC."), NULL);
#endif
  gtk_entry_set_text (GTK_ENTRY (TimeZoneEntry), g_key_file_get_value(GUISettings, "default", "timezone", NULL));
  gtk_entry_set_width_chars (GTK_ENTRY (TimeZoneEntry), 7);

  /* Toggle visibility of time zone entry when auto time zone is toggled */
  timezone_toggle_visibility(AutoTimeZoneCheck, TimeZoneEntry);
  g_signal_connect(AutoTimeZoneCheck, "toggled",
                   G_CALLBACK (timezone_toggle_visibility), GTK_ENTRY (TimeZoneEntry));

  PhotoOffsetEntry = gtk_entry_new ();
  gtk_widget_show (PhotoOffsetEntry);
  gtk_table_attach (GTK_TABLE (OptionsTable), PhotoOffsetEntry, 1, 2, 2, 3,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (PhotoOffsetEntry,
	_("The number of seconds to add to the photo's time to make it match "
	  "the GPS data. Calculate this with (GPS - Photo). "
	  "Can be negative or positive."));
#else
  gtk_tooltips_set_tip (tooltips, PhotoOffsetEntry,
	_("The number of seconds to add to the photo's time to make it match "
	  "the GPS data. Calculate this with (GPS - Photo). "
	  "Can be negative or positive."), NULL);
#endif
  gtk_entry_set_text (GTK_ENTRY (PhotoOffsetEntry), g_key_file_get_value(GUISettings, "default", "photooffset", NULL));
  gtk_entry_set_width_chars (GTK_ENTRY (PhotoOffsetEntry), 7);

  GPSDatumEntry = gtk_entry_new ();
  gtk_widget_show (GPSDatumEntry);
  gtk_table_attach (GTK_TABLE (OptionsTable), GPSDatumEntry, 1, 2, 4, 5,
                    (GtkAttachOptions) (0),
                    (GtkAttachOptions) (0), 0, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (GPSDatumEntry,
	_("The datum used for the GPS data. This text here is recorded in the "
	  "EXIF tags as the source datum. WGS-84 is very commonly used."));
#else
  gtk_tooltips_set_tip (tooltips, GPSDatumEntry,
	_("The datum used for the GPS data. This text here is recorded in the "
	  "EXIF tags as the source datum. WGS-84 is very commonly used."), NULL);
#endif
  gtk_entry_set_text (GTK_ENTRY (GPSDatumEntry), g_key_file_get_value(GUISettings, "default", "gpsdatum", NULL));
  gtk_entry_set_width_chars (GTK_ENTRY (GPSDatumEntry), 7);

  OptionsFrameLable = gtk_label_new (_("<b>3. Set options</b>"));
  gtk_widget_show (OptionsFrameLable);
  gtk_frame_set_label_widget (GTK_FRAME (OptionsFrame), OptionsFrameLable);
  gtk_label_set_use_markup (GTK_LABEL (OptionsFrameLable), TRUE);

  /* Correlate button area. */
  CorrelateFrame = gtk_frame_new (NULL);
  gtk_widget_show (CorrelateFrame);
  gtk_box_pack_start (GTK_BOX (ControlsVBox), CorrelateFrame, FALSE, FALSE, 0);

  CorrelateAlignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (CorrelateAlignment);
  gtk_container_add (GTK_CONTAINER (CorrelateFrame), CorrelateAlignment);
  gtk_alignment_set_padding (GTK_ALIGNMENT (CorrelateAlignment), 0, 4, 12, 4);

  CorrelateButton = gtk_button_new_with_mnemonic (_("Correlate Photos"));
  gtk_widget_show (CorrelateButton);
  gtk_container_add (GTK_CONTAINER (CorrelateAlignment), CorrelateButton);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (CorrelateButton,
	_("Begin the correlation process, writing back into the photos' "
	  "EXIF tags (unless Don't write is selected)."));
#else
  gtk_tooltips_set_tip (tooltips, CorrelateButton,
	_("Begin the correlation process, writing back into the photos' "
	  "EXIF tags (unless Don't write is selected)."), NULL);
#endif
  g_signal_connect (G_OBJECT (CorrelateButton), "clicked",
  		G_CALLBACK (CorrelateButtonPress), NULL);

  CorrelateLabel = gtk_label_new (_("<b>4. Correlate!</b>"));
  gtk_widget_show (CorrelateLabel);
  gtk_frame_set_label_widget (GTK_FRAME (CorrelateFrame), CorrelateLabel);
  gtk_label_set_use_markup (GTK_LABEL (CorrelateLabel), TRUE);
  
  /* Other options area. */
  OtherOptionsFrame = gtk_frame_new (NULL);
  gtk_widget_show (OtherOptionsFrame);
  gtk_box_pack_start (GTK_BOX (ControlsVBox), OtherOptionsFrame, FALSE, FALSE, 0);

  OtherOptionsAlignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (OtherOptionsAlignment);
  gtk_container_add (GTK_CONTAINER (OtherOptionsFrame), OtherOptionsAlignment);
  gtk_alignment_set_padding (GTK_ALIGNMENT (OtherOptionsAlignment), 0, 4, 12, 4);

  OtherOptionsVBox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (OtherOptionsVBox);
  gtk_container_add (GTK_CONTAINER (OtherOptionsAlignment), OtherOptionsVBox);

  StripGPSButton = gtk_button_new_with_mnemonic (_("Strip GPS tags"));
  gtk_widget_show (StripGPSButton);
  gtk_box_pack_start (GTK_BOX (OtherOptionsVBox), StripGPSButton, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (StripGPSButton,
	_("Strip GPS tags from the selected photos."));
#else
  gtk_tooltips_set_tip (tooltips, StripGPSButton,
	_("Strip GPS tags from the selected photos."), NULL);
#endif
  g_signal_connect (G_OBJECT (StripGPSButton), "clicked",
  		G_CALLBACK (StripGPSButtonPress), NULL);

  AboutButton = gtk_button_new_with_mnemonic (_("About"));
  gtk_widget_show (AboutButton);
  gtk_box_pack_start (GTK_BOX (OtherOptionsVBox), AboutButton, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION(2, 12, 0)
  gtk_widget_set_tooltip_text (AboutButton,
	_("Show information about the program."));
#else
  gtk_tooltips_set_tip (tooltips, AboutButton,
	_("Show information about the program."), NULL);
#endif
  g_signal_connect (G_OBJECT (AboutButton), "clicked",
  		G_CALLBACK (AboutButtonPress), NULL);

  OtherOptionsLabel = gtk_label_new (_("<b>Other Tools</b>"));
  gtk_widget_show (OtherOptionsLabel);
  gtk_frame_set_label_widget (GTK_FRAME (OtherOptionsFrame), OtherOptionsLabel);
  gtk_label_set_use_markup (GTK_LABEL (OtherOptionsLabel), TRUE);

  /* Photo list box area of the window. */
  PhotoListVBox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (PhotoListVBox);
  gtk_box_pack_start (GTK_BOX (WindowHBox), PhotoListVBox, TRUE, TRUE, 0);

  PhotoListScroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (PhotoListScroll);
  gtk_box_pack_start (GTK_BOX (PhotoListVBox), PhotoListScroll, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (PhotoListScroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (PhotoListScroll), GTK_SHADOW_IN);

  /* Get the photo list store ready. */
  PhotoListStore = gtk_list_store_new(LIST_NOCOLUMNS,
		  G_TYPE_STRING,  /* The Filename */
		  G_TYPE_STRING,  /* Latitude     */
		  G_TYPE_STRING,  /* Longitude    */
		  G_TYPE_STRING,  /* Elevation    */
		  G_TYPE_STRING,  /* The Time     */
		  G_TYPE_STRING,  /* The State    */
		  G_TYPE_POINTER); /* Pointer to the matching list item. */

  PhotoList = gtk_tree_view_new_with_model (GTK_TREE_MODEL(PhotoListStore));
  gtk_widget_show (PhotoList);
  gtk_container_add (GTK_CONTAINER (PhotoListScroll), PhotoList);

  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(PhotoList)), GTK_SELECTION_MULTIPLE);
  
  /* Prepare the columns. We need columns. Columns are good. */
  PhotoListRenderer = gtk_cell_renderer_text_new ();

  /* File Column. */
  FileColumn = gtk_tree_view_column_new_with_attributes (_("File"), PhotoListRenderer,
							"text", LIST_FILENAME,
							NULL);
  gtk_tree_view_column_set_resizable (FileColumn, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (PhotoList), FileColumn);

  /* Latitude Column. */
  LatColumn = gtk_tree_view_column_new_with_attributes (_("Latitude"), PhotoListRenderer,
							"text", LIST_LAT,
							NULL);
  gtk_tree_view_column_set_resizable (LatColumn, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (PhotoList), LatColumn);
  
  /* Longitude Column. */
  LongColumn = gtk_tree_view_column_new_with_attributes (_("Longitude"), PhotoListRenderer,
							"text", LIST_LONG,
							NULL);
  gtk_tree_view_column_set_resizable (LongColumn, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (PhotoList), LongColumn);
  
  /* Elevation Column. */
  ElevColumn = gtk_tree_view_column_new_with_attributes (_("Elevation"), PhotoListRenderer,
							"text", LIST_ELEV,
							NULL);
  gtk_tree_view_column_set_resizable (ElevColumn, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (PhotoList), ElevColumn);
  
  /* Time column. */
  TimeColumn = gtk_tree_view_column_new_with_attributes (_("Time"), PhotoListRenderer,
							"text", LIST_TIME,
							NULL);
  gtk_tree_view_column_set_resizable (TimeColumn, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (PhotoList), TimeColumn);

  /* State column. */
  StateColumn = gtk_tree_view_column_new_with_attributes (_("State"), PhotoListRenderer,
							"text", LIST_STATE,
							NULL);
  gtk_tree_view_column_set_resizable (StateColumn, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (PhotoList), StateColumn);

  /* Get the track list store ready.
   * Create the empty terminating entry. */
  GPSData = (struct GPSTrack*) calloc(1, sizeof(*GPSData));
  NumTracks = 0;

  /* Final thing: show the window. */
  gtk_widget_show(MatchWindow);

  /* Done! Return a pointer to the window, although we never use it... */
  return MatchWindow;
}

gboolean DestroyWindow(GtkWidget *Widget,
		GdkEvent *Event,
		gpointer Data)
{
	(void) Widget;  // Unused
	(void) Event;   // Unused
	(void) Data;    // Unused

	/* Record the settings, and then save them. */
	g_key_file_set_boolean(GUISettings, "default", "interpolate", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(InterpolateCheck)));
	g_key_file_set_boolean(GUISettings, "default", "dontwrite", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NoWriteCheck)));
	g_key_file_set_boolean(GUISettings, "default", "replace", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(OverwriteCheck)));
	g_key_file_set_boolean(GUISettings, "default", "nochangemtime", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NoMtimeCheck)));
	g_key_file_set_boolean(GUISettings, "default", "betweensegments", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(BetweenSegmentsCheck)));
	g_key_file_set_boolean(GUISettings, "default", "writeddmmss", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(DegMinSecsCheck)));
	g_key_file_set_boolean(GUISettings, "default", "autotimezone", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AutoTimeZoneCheck)));
	g_key_file_set_string(GUISettings, "default", "maxgap", gtk_entry_get_text(GTK_ENTRY(GapTimeEntry)));
	g_key_file_set_string(GUISettings, "default", "timezone", gtk_entry_get_text(GTK_ENTRY(TimeZoneEntry)));
	g_key_file_set_string(GUISettings, "default", "photooffset", gtk_entry_get_text(GTK_ENTRY(PhotoOffsetEntry)));
	g_key_file_set_string(GUISettings, "default", "gpsdatum", gtk_entry_get_text(GTK_ENTRY(GPSDatumEntry)));
	if (GPXOpenDir)
	    g_key_file_set_string(GUISettings, "default", "gpxopendir", GPXOpenDir);
	if (PhotoOpenDir)
		g_key_file_set_string(GUISettings, "default", "photoopendir", PhotoOpenDir);
	SaveSettings();

	/* Someone closed the window. */
	/* Free the memory we allocated for the photo list. */
	struct GUIPhotoList* Free = NULL;
	struct GUIPhotoList* Free2 = NULL;
	if (FirstPhoto)
	{
		/* Walk through the singly-linked list
		 * freeing stuff. */
		Free = FirstPhoto;
		while (1)
		{
			free(Free->Filename);
			free(Free->Time);
			Free2 = Free->Next;
			free(Free);
			if (Free2 == NULL) break;
			Free = Free2;
		}
	}
	
	/* Free the memory for the GPS data, if applicable. */
	while (NumTracks > 0)
	{
		--NumTracks;
		FreeTrack(&GPSData[NumTracks]);
	}
	free(GPSData);

	/* Tell GTK that we're done. */
#if GTK_CHECK_VERSION(2, 12, 0)
	exit(0);
#else
	gtk_exit(0);
#endif

	/* And return FALSE so that GTK knows we have not
	 * vetoed the close. */
	return FALSE;
}                                                

void AddPhotosButtonPress( GtkWidget *Widget, gpointer Data )
{
	(void) Widget;  // Unused
	(void) Data;    // Unused

	/* Add some photos to this thing. */
	GtkWidget *AddPhotosDialog;

	/* Get the dialog ready. */
	AddPhotosDialog = gtk_file_chooser_dialog_new (_("Add Photos..."),
			GTK_WINDOW(MatchWindow),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(AddPhotosDialog), TRUE);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(AddPhotosDialog), PhotoOpenDir);

	GtkFileFilter *JpgFilter = gtk_file_filter_new();
	if (JpgFilter) {
		gtk_file_filter_add_pattern(JpgFilter, "*.[jJ][pP][gG]");
		gtk_file_filter_add_pattern(JpgFilter, "*.[jJ][pP][eE][gG]");
		gtk_file_filter_set_name(JpgFilter, _("JPEG images"));
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(AddPhotosDialog), JpgFilter);
	}
	GtkFileFilter *AllFilter = gtk_file_filter_new();
	if (AllFilter) {
		gtk_file_filter_add_pattern(AllFilter, "*");
		gtk_file_filter_set_name(AllFilter, _("All files"));
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(AddPhotosDialog), AllFilter);
	}

	/* Run the dialog. */
	if (gtk_dialog_run (GTK_DIALOG (AddPhotosDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/* Hide the dialog. */
		gtk_widget_hide(AddPhotosDialog);
		/* Haul out the selected files. 
		 * We pass them along to another function that will
		 * add them to the internal list and onto the screen. */
		/* GTK returns a GSList - a singly-linked list of filenames. */
		GSList* FileNames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(AddPhotosDialog));
		GSList* Run;
		for (Run = FileNames; Run; Run = Run->next)
		{
			/* Show what's happening on the screen. */
			GtkGUIUpdate();
			/*printf("Filename: %s.\n", (char*)Run->data);*/
			/* Call the other function with the filename - this
			 * function adds it to the internal list, and adds it
			 * to the screen display, too. */
			AddPhotoToList((char*)Run->data);
			/* Free the memory passed to us. */
			g_free(Run->data);
		}
		/* We're done with the list - free it. */
		g_slist_free(FileNames);
	}

	/* Copy out the directory that the user ended up at. */
	g_free(PhotoOpenDir);
	PhotoOpenDir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(AddPhotosDialog));

	/* Now we're done with the dialog. See you! */
	gtk_widget_destroy (AddPhotosDialog);

}

void AddPhotoToList(const char* Filename)
{
	/* Add the photo to the list. Query out the exif tags
	 * at the same time, too - so we can add them to the
	 * current list. */
	/* Note that this function does more than update the GUI:
	 * it also adds it to the internal list, ready to go. */
	
	/* Get ready to read the relevant data. */
	GtkTreeIter AddStuff;

	double Lat, Long, Elev;
	Lat = Long = Elev = 0;
	int IncludesGPS = 0;

	/* Read the EXIF data. */
	char *Time = ReadExifData(Filename, &Lat, &Long, &Elev, &IncludesGPS);

	/* Note: we don't check if Time is NULL here. It is done for
	 * us in SetListItem, and we check again before we attempt
	 * to allocate memory to store "Time" in. */
		
	/* Add the data to the list. */
	gtk_list_store_append(PhotoListStore, &AddStuff);
	SetListItem(&AddStuff, Filename, Time, Lat, Long, Elev, NULL, IncludesGPS);

	/* Save away the filename and the TreeIter information in the internal
	 * singly-linked list. */
	/* We also make a copy of Filename - it won't exist once we return. */
	if (FirstPhoto)
	{
		/* Already at least one element. Add to it. */
		LastPhoto->Next = (struct GUIPhotoList*) malloc(sizeof(struct GUIPhotoList));
		LastPhoto = LastPhoto->Next;
		LastPhoto->Next = NULL;
	} else {
		/* No elements. Righto, add one. */
		FirstPhoto = (struct GUIPhotoList*) malloc(sizeof(struct GUIPhotoList));
		LastPhoto = FirstPhoto;
		FirstPhoto->Next = NULL;
	}

	/* Now that we've allocated memory for the structure, allocate
	 * memory for the strings and then fill them. */
	/* Filename first... */
	LastPhoto->Filename = strdup(Filename);
	/* And then Time, after checking for NULLness. */
	if (Time)
	{
		LastPhoto->Time = strdup(Time);
	} else {
		LastPhoto->Time = strdup(_("No EXIF data"));
	}
	/* Save the TreeIter as the last step. */
	LastPhoto->ListPointer = AddStuff;
	
	/* Save the pointer into the data, as well. */
	gtk_list_store_set(PhotoListStore, &AddStuff,
		LIST_POINTER, LastPhoto,
		-1);

	/* Free the memory allocated for us.
	 * (ReadExifData allocates and returns memory) */
	free(Time);
}

static void gtk_tree_path_free_(gpointer path, gpointer data)
{
	(void) data;  // Unused
	gtk_tree_path_free((GtkTreePath *)path);
}

void RemovePhotosButtonPress( GtkWidget *Widget, gpointer Data )
{
	(void) Widget;  // Unused
	(void) Data;    // Unused

	/* Someone clicked the remove photos button. So make it happen!
	 * First, query out what was selected. */
	GtkTreeIter Iter;

	GtkTreeSelection* Selection;
	Selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(PhotoList));

	GList* Selected = gtk_tree_selection_get_selected_rows(Selection, NULL);

	/* Sanity check: was anything selected? */
	if (Selected == NULL)
	{
		/* Nothing is selected. Do nothing. */
		return;
	}

	/* Count the items on the GList. We need this to be able to
	 * keep a list of items that have been removed from our internal
	 * list, and should be removed from the screen. */
	int SelectedCount = 0;
	GList* Walk;
	for (Walk = Selected; Walk; Walk = Walk->next)
	{
		SelectedCount++;
	}

	/* Now get ready to keep a list of Iters that we can
	 * delete. */
	GtkTreeIter* RemoveIters = (GtkTreeIter*) malloc(sizeof(GtkTreeIter) * SelectedCount);

	/* Walk through and remove the items from our internal list. */
	struct GUIPhotoList* PhotoWalk = NULL;
	struct GUIPhotoList* LastPhotoWalk = NULL;
	struct GUIPhotoList* FreeHold = NULL;
	struct GUIPhotoList* FreeMe = NULL;
	int IterCount = 0;
	for (Walk = Selected; Walk; Walk = Walk->next)
	{
		/* Acquire a new Iter for this selected row. */
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(PhotoListStore), &Iter,
					    (GtkTreePath*) Walk->data))
		{
			/* To remove the row from our internal list.
			 * Locate it via the pointer stored. */
			gtk_tree_model_get(GTK_TREE_MODEL(PhotoListStore), &Iter,
					LIST_POINTER, &FreeMe, -1);
			/* Save the iter for afterwards: we can then remove the
			 * data from the screen. */
			/* gtk_list_store_remove(PhotoListStore, &Iter); */
			RemoveIters[IterCount] = Iter;
			IterCount++;
			/* Now go back through and take it out from our list. */
			PhotoWalk = FirstPhoto;
			LastPhotoWalk = NULL;
			while (1)
			{
				if (PhotoWalk == NULL) break;
				/*printf("Search: %d / %d.\n", PhotoWalk, FreeMe);*/
				/* Check to see if this is the one we want to be rid of. */
				if (PhotoWalk == FreeMe)
				{
					/* Right. Remove. */
					/*printf("Removing: %s.\n", PhotoWalk->Filename);*/
					PhotoWalk = PhotoWalk->Next;
					if (FreeMe == FirstPhoto)
					{
						/* Remove the top of the list. */
						FreeHold = FreeMe;
						FirstPhoto = FreeMe->Next;
						LastPhotoWalk = FreeMe->Next;
						free(FreeHold->Filename);
						free(FreeHold->Time);
						free(FreeHold);
						break;
					} else if (FreeMe == LastPhoto) {
						/* Remove the bottom of the list. */
						FreeHold = FreeMe;
						LastPhoto = LastPhotoWalk;
						LastPhoto->Next = NULL;
						free(FreeHold->Filename);
						free(FreeHold->Time);
						free(FreeHold);
						break;
					} else {
						/* Remove a middle of the list. */
						FreeHold = FreeMe;
						LastPhotoWalk->Next = FreeHold->Next;
						free(FreeHold->Filename);
						free(FreeHold->Time);
						free(FreeHold);
						break;
					}
				} else {
					/* Nope, this wasn't what we wanted to delete.
					 * So get ready to look at the next one, keeping
					 * mind of where we were. */
					LastPhotoWalk = PhotoWalk;
					PhotoWalk = PhotoWalk->Next;
				}
			} /* End for Walk the photo list. */
			
		}
	} /* End for Walk the GList. */

	/* Now remove the rows from the screen. 
	 * By this point, they are no longer in our internal list. */
	int i;
	for (i = 0; i < SelectedCount; i++)
	{
		gtk_list_store_remove(PhotoListStore, &RemoveIters[i]);
	}
	free(RemoveIters);

	/* Free the memory used by GList. */
	g_list_foreach(Selected, gtk_tree_path_free_, NULL);
	g_list_free(Selected);

	/* Debug: walk the photo list tree. */
	/*struct GUIPhotoList* List;
	for (List = FirstPhoto; List; List = List->Next)
	{
		printf("List Filename: %s\n", List->Filename);
	}
	printf("--------------------------------------------------------------\n");*/

}

void SetListItem(GtkTreeIter* Iter, const char* Filename, const char* Time, double Lat,
		double Long, double Elev, const char* PassedState, int IncludesGPS)
{
	/* Scratch areas. */
	char LatScratch[100] = "";
	char LongScratch[100] = "";
	char ElevScratch[100] = "";
	const char* State = NULL;
	
	/* Format all the data. */
	if (!Time)
	{
		/* Not good. Failure. */
		Time = "";
		State = _("No EXIF data");
	} else {
		/* All ok. Get ready. */
		if (IncludesGPS)
		{
			State = _("GPS Data Present");
			/* In each case below, consider the values
			 * that are invalid for each - if that's the case,
			 * consider the spots as "blank". */
			/* Lat can't be greater than 90 degrees. */
			if (Lat < 200)
			{
				snprintf(LatScratch, sizeof(LatScratch), "%f (%s)",
					Lat, (Lat < 0) ? _("S") : _("N"));
			} else {
				snprintf(LatScratch, sizeof(LatScratch), " ");
			}
			/* Long can't be greater than 180 degrees. */
			if (Long < 200)
			{
				snprintf(LongScratch, sizeof(LongScratch), "%f (%s)",
					Long, (Long < 0) ? _("W") : _("E"));
			} else {
				snprintf(LongScratch, sizeof(LongScratch), " ");
			}
			/* Radius of earth ~6000km */
			if (Elev > -7000000)
			{
				snprintf(ElevScratch, sizeof(ElevScratch), "%.2f%s", Elev, _("m"));
			} else {
				snprintf(ElevScratch, sizeof(ElevScratch), " ");
			}
		} else {
			/* Placeholders for the lack of data. */
			State = _("Ready");
		}
	}

	/* Overwrite state with what we want, if needed. */
	if (PassedState) State = PassedState;
	/* And set all the appropriate data. */
	gtk_list_store_set(PhotoListStore, Iter,
		LIST_FILENAME, strrchr(Filename, G_DIR_SEPARATOR)+1,
		LIST_LAT, LatScratch,
		LIST_LONG, LongScratch,
		LIST_ELEV, ElevScratch,
		LIST_TIME, Time,
		LIST_STATE, State,
		-1);
	
}

void SetState(GtkTreeIter* Iter, const char* State)
{
	/* Set the state on the item... just the state. */
	gtk_list_store_set(PhotoListStore, Iter,
		LIST_STATE, State,
		-1);
}


void SelectGPSButtonPress( GtkWidget *Widget, gpointer Data )
{
	(void) Widget;  // Unused
	(void) Data;    // Unused

	/* Select and load some GPS data! */
	GtkWidget *GPSDataDialog;
	GtkWidget *ErrorDialog;
	
	/* Get the dialog ready... */
	GPSDataDialog = gtk_file_chooser_dialog_new (_("Select GPS Data..."),
			GTK_WINDOW(MatchWindow),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(GPSDataDialog), TRUE);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(GPSDataDialog), GPXOpenDir);

	GtkFileFilter *GpxFilter = gtk_file_filter_new();
	if (GpxFilter) {
		gtk_file_filter_add_pattern(GpxFilter, "*.[gG][pP][xX]");
		gtk_file_filter_set_name(GpxFilter, _("GPX files"));
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(GPSDataDialog), GpxFilter);
	}
	GtkFileFilter *AllFilter = gtk_file_filter_new();
	if (AllFilter) {
		gtk_file_filter_add_pattern(AllFilter, "*");
		gtk_file_filter_set_name(AllFilter, _("All files"));
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(GPSDataDialog), AllFilter);
	}

	/* Run the dialog... */
	if (gtk_dialog_run (GTK_DIALOG (GPSDataDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/* Sanity check: free the GPS tracks in case we already have one.
		 * Note: we check this now, because if we cancelled the dialog,
		 * we should run with the old data. */
		while (NumTracks > 0)
		{
			--NumTracks;
			FreeTrack(&GPSData[NumTracks]);
		}

		/* Hide the "open" dialog. */
		gtk_widget_hide(GPSDataDialog);

		/* Display a dialog so the user knows whats going down. */
		ErrorDialog = gtk_message_dialog_new (GTK_WINDOW(MatchWindow),
			(GtkDialogFlags) (GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL),
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_NONE,
			_("Loading GPS data from file... Won't be a moment..."));
		gtk_widget_show(ErrorDialog);
		GtkGUIUpdate();

		char *FirstOrBadFileName = NULL;
		int ReadOk = 1;

		/* GTK returns a GSList - a singly-linked list of filenames. */
		/* Process the result of the dialog... */
		GSList* FileNames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(GPSDataDialog));
		GSList* Run;
		for (Run = FileNames; Run; Run = Run->next)
		{
			/* Read in the new data, but stop trying after the first failure. */
			if (ReadOk)
			{
				if (FirstOrBadFileName == NULL)
				{
					/* If only one file is given, this is it */
					FirstOrBadFileName = strdup((char *)Run->data);
				}
				else
				{
					/* If more than one file is given, say so */
					free(FirstOrBadFileName);
					/* This string must look like a file path */
					FirstOrBadFileName = strdup(_(G_DIR_SEPARATOR_S "multiple files"));
				}

				ReadOk = ReadGPX((char*)Run->data, &GPSData[NumTracks]);
				if (ReadOk)
				{
					/* Make room for a new end-of-array entry */
					++NumTracks;
					GPSData = (struct GPSTrack*) realloc(GPSData, sizeof(*GPSData)*(NumTracks+1));
					memset(&GPSData[NumTracks], 0, sizeof(*GPSData));
				} else
				{
					/* If a file could not be read, give the name */
					free(FirstOrBadFileName);
					FirstOrBadFileName = strdup((char *)Run->data);
				}
			}

			/* Free the memory passed to us. */
			g_free(Run->data);
		}

		/* We're done with the list - free it. */
		g_slist_free(FileNames);

		/* Close the dialog now that we're done. */
		gtk_widget_destroy(ErrorDialog);

		/* Prepare our "scratch" for rewriting labels. */
		const size_t ScratchLength = strlen(FirstOrBadFileName) + 100;
		char* Scratch = (char*) malloc(sizeof(char) * ScratchLength);

		/* Check if all the data was read ok. */
		if (ReadOk)
		{
			/* It's all good!
			 * Adjust the label to say so. */
			snprintf(Scratch, ScratchLength,
				 _("Read from: %s"), strrchr(FirstOrBadFileName, G_DIR_SEPARATOR)+1);
			gtk_label_set_text(GTK_LABEL(GPSSelectedLabel), Scratch);
		} else {
			/* Not good. Say so. */
			/* Set the label... */
			snprintf(Scratch, ScratchLength, _("Read from: No file"));
			gtk_label_set_text(GTK_LABEL(GPSSelectedLabel), Scratch);
			/* Show an error dialog. */
			ErrorDialog = gtk_message_dialog_new (GTK_WINDOW(MatchWindow),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					_("Unable to read file %s for some reason. Please try again."),
					FirstOrBadFileName);
			gtk_dialog_run (GTK_DIALOG (ErrorDialog));
			gtk_widget_destroy (ErrorDialog);

			/* Clean up the tracks already read in */
			while (NumTracks > 0)
			{
				--NumTracks;
				FreeTrack(&GPSData[NumTracks]);
			}
		}
		
		/* Clean up... */
		free(Scratch);
		free(FirstOrBadFileName);
	}

	/* Make a note of the directory we stopped at. */
	g_free(GPXOpenDir);
	GPXOpenDir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(GPSDataDialog));
	
	/* Now we're finished with the dialog... free it. */
	gtk_widget_destroy (GPSDataDialog);

}

void CorrelateButtonPress( GtkWidget *Widget, gpointer Data )
{
	(void) Widget;  // Unused
	(void) Data;    // Unused

	/* We were asked to correlate some photos... make it happen... */
	GtkWidget *ErrorDialog;

	/* Check to see we have everything we need... */
	if (FirstPhoto == NULL)
	{
		/* No photos... */
		ErrorDialog = gtk_message_dialog_new (GTK_WINDOW(MatchWindow),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("No photos selected to match! Please use Add to add photos first!"));
		gtk_dialog_run (GTK_DIALOG (ErrorDialog));
		gtk_widget_destroy (ErrorDialog);
		return;
	}

	if (NumTracks == 0)
	{
		/* No GPS data... */
		ErrorDialog = gtk_message_dialog_new (GTK_WINDOW(MatchWindow),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("No GPS data loaded! Please select a GPX file to read GPS data from."));
		gtk_dialog_run (GTK_DIALOG (ErrorDialog));
		gtk_widget_destroy (ErrorDialog);
		return;
	}

	/* Assemble the settings for the correlation run. */
	struct CorrelateOptions Options;

	/* Interpolation. */
	/* This is confusing. I should have thought more about the Interpolate
	 * flags in the CorrelateOptions structure. But, if you think about
	 * it for a bit, it can make sense. Enough sense to use.  */
	Options.NoInterpolate = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(InterpolateCheck));

	/* Write or no write. */
	Options.NoWriteExif = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NoWriteCheck));

	/* Force overwrite. */
	Options.OverwriteExisting = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(OverwriteCheck));

	/* No change MTime. */
	Options.NoChangeMtime = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NoMtimeCheck));

	/* Between segments? */
	Options.DoBetweenTrkSeg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(BetweenSegmentsCheck));

	/* DD MM.MM or DD MM SS.SS? */
	Options.DegMinSecs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(DegMinSecsCheck));
	
	/* Feather time. */
	Options.FeatherTime = atof(gtk_entry_get_text(GTK_ENTRY(GapTimeEntry)));

	/* GPS Datum. */
	Options.Datum = strdup(gtk_entry_get_text(GTK_ENTRY(GPSDatumEntry)));
		
	/* TimeZone. We may need to extract the timezone from a string. */
	Options.AutoTimeZone = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(AutoTimeZoneCheck));
	Options.TimeZoneHours = 0;
	Options.TimeZoneMins = 0;
	char* TZString = (char*) gtk_entry_get_text(GTK_ENTRY(TimeZoneEntry));
	/* Check the string. If there is a colon, then it's a time in xx:xx format.
	 * If not, it's probably just a +/-xx format. In all other cases,
	 * it will be interpreted as +/-xx, which, if given a string, returns 0. */
	if (strstr(TZString, ":"))
	{
		/* Found colon. Split into two. */
		sscanf(TZString, "%d:%d", &Options.TimeZoneHours, &Options.TimeZoneMins);
		if (Options.TimeZoneHours < 0)
		    Options.TimeZoneMins *= -1;
	} else {
		/* No colon. Just parse. */
		Options.TimeZoneHours = atoi(TZString);
	}

	/* Photo Offset time */
	Options.PhotoOffset = atoi(gtk_entry_get_text(GTK_ENTRY(PhotoOffsetEntry)));

	/* Store the GPS track */
	Options.Track = GPSData;

	/* Walk through the list, correlating, and updating the screen. */
	struct GUIPhotoList* Walk;
	struct GPSPoint* Result;
	const char* State = _("Internal error");
	GtkTreePath* ShowPath;
	for (Walk = FirstPhoto; Walk; Walk = Walk->Next)
	{
		/* Say that we're doing it... */
		SetState(&Walk->ListPointer, _("Correlating..."));

		/* Point to the cell, too... ie, scroll the tree view
		 * to ensure that the one we're playing with can be seen on screen. */
		ShowPath = gtk_tree_model_get_path(GTK_TREE_MODEL(PhotoListStore), &Walk->ListPointer);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(PhotoList),
				ShowPath, NULL, FALSE, 0, 0);
		gtk_tree_path_free(ShowPath);
		GtkGUIUpdate();
		
		/* Do the correlation. */
		Result = CorrelatePhoto(Walk->Filename, &Options);
		
		/* Figure out if it worked. */
		if (Result)
		{
			/* Result was not null. That means that we
			 * matched to a point. But that's not the whole
			 * story. Read on... */
			switch (Options.Result)
			{
				case CORR_OK:
					/* All cool! Exact match! */
					State = _("Exact Match");
					break;
				case CORR_INTERPOLATED:
					/* All cool! Interpolated match. */
					State = _("Interpolated Match");
					break;
				case CORR_ROUND:
					/* All cool! Rounded match. */
					State = _("Rounded Match");
					break;
				case CORR_EXIFWRITEFAIL:
					/* Not cool - matched, not written. */
					State = _("Write Failure");
					break;
			}
			/* Now update the screen with the numbers. */
			SetListItem(&Walk->ListPointer, Walk->Filename,
					Walk->Time, Result->Lat, Result->Long,
					Result->Elev, State, 1);
		} else {
			/* Result was null. This means something
			 * really went wrong. Find out and put that
			 * on the screen. */
			if (Options.Result == CORR_GPSDATAEXISTS)
			{
				/* Do nothing... */
				SetState(&Walk->ListPointer, _("Data Already Present"));
				continue;
			}
			switch (Options.Result)
			{
				case CORR_NOMATCH:
					/* No match: outside data. */
					State = _("No Match");
					break;
				case CORR_TOOFAR:
					/* Too far from any point. */
					State = _("Too far");
					break;
				case CORR_NOEXIFINPUT:
					/* No exif data input. */
					State = _("No data");
					break;
			}
			/* Now update the screen with the changed state. */
			SetListItem(&Walk->ListPointer, Walk->Filename,
					Walk->Time, 0, 0, 0,
					State, 0);
		} /* End if Result */
	} /* End for Walk the list ... */

	free(Options.Datum);
}

void StripGPSButtonPress( GtkWidget *Widget, gpointer Data )
{
	(void) Widget;  // Unused
	(void) Data;    // Unused

	/* Someone clicked the Strip GPS Data button. So make it happen!
	 * First, query out what was selected. */
	GtkTreeIter Iter;

	GtkTreeSelection* Selection;
	Selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(PhotoList));

	GList* Selected = gtk_tree_selection_get_selected_rows(Selection, NULL);

	/* Sanity check: was anything selected? */
	if (Selected == NULL)
	{
		/* Nothing is selected. Do nothing. */
		return;
	}

	int NoChangeMtime = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NoMtimeCheck));
	int NoWriteExif = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(NoWriteCheck));

	/* Walk through and remove the items from our internal list. */
	GList* Walk;
	struct GUIPhotoList* PhotoData = NULL;
	GtkTreePath* ShowPath;
	for (Walk = Selected; Walk; Walk = Walk->next)
	{
		/* Get an Iter for this selected row. */
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(PhotoListStore), &Iter,
					    (GtkTreePath*) Walk->data)) {
			/* Fetch out the data... */
			gtk_tree_model_get(GTK_TREE_MODEL(PhotoListStore), &Iter, LIST_POINTER, &PhotoData, -1);
		} else {
			/* Unable to get the iter...
			 * Try again, later. */
			continue;
		}

		/* Say that we're doing it... */
		SetState(&PhotoData->ListPointer, _("Stripping..."));

		/* Point to the cell, too... ie, scroll the tree view
		 * to ensure that the one we're playing with can be seen on screen. */
		ShowPath = gtk_tree_model_get_path(GTK_TREE_MODEL(PhotoListStore), &PhotoData->ListPointer);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(PhotoList),
				ShowPath, NULL, FALSE, 0, 0);
		gtk_tree_path_free(ShowPath);
		GtkGUIUpdate();
		
		/* Strip the tags. */
		if (RemoveGPSExif(PhotoData->Filename, NoChangeMtime, NoWriteExif))
		{
			SetListItem(&PhotoData->ListPointer, PhotoData->Filename,
				PhotoData->Time, 200, 200, -7000000, "", 1);
		} else {
			SetListItem(&PhotoData->ListPointer, PhotoData->Filename,
				PhotoData->Time, 200, 200, -7000000, _("Error Stripping"), 1);
		}

	} /* End for Walk the GList. */

	/* Debug: walk the photo list tree. */
	/*struct GUIPhotoList* List;
	for (List = FirstPhoto; List; List = List->Next)
	{
		printf("List Filename: %s\n", List->Filename);
	}
	printf("--------------------------------------------------------------\n");*/

}

void AboutButtonPress( GtkWidget *Widget, gpointer Data )
{
	static const gchar * const authors[] = {
		"Daniel Foote",
		"Dan Fandrich",
		NULL,
	};
	(void) Data;    // Unused
	GtkWidget *Toplevel = gtk_widget_get_toplevel (Widget);
	gtk_show_about_dialog(GTK_WINDOW(Toplevel),
						"authors", authors,
						"comments", _("GPS Correlate attaches EXIF GPS location tags to images."),
						// The following hex bytes are the copyright symbol in UTF-8
						"copyright", _("Copyright \xC2\xA9 2005-2019 Daniel Foote, Dan Fandrich"),
						"license", "GPL 2+",
						"logo-icon-name", "gpscorrelate-gui",
						"version", PACKAGE_VERSION,
						"website", "https://dfandrich.github.io/gpscorrelate/",
NULL);
}

void GtkGUIUpdate(void)
{
	/* Process all GUI events that need to happen. */
	/* This lets us "update" the screen while things are
	 * in motion. Might generate slowdowns with heaps of data,
	 * but generally, it's a good thing. */
	while (gtk_events_pending ())
		gtk_main_iteration ();
}
