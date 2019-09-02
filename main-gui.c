/* main-gui.c
 * Written by Daniel Foote.
 * Started Feb 2005.
 *
 * GTK GUI program to correlate photos with GPS data.
 * Uses common parts of a command line version of the same.
 * Just this time, with a pretty GUI to make it all simple and easy. */

/* Although I did not start out with a clear idea of the contents
 * of this file, I had thought it would be a little more than a "stub"
 * to get the GUI up and running. However, this seems to work for me!
 * All the action really happens in gui.c. */

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

#include <stdlib.h>

#include <gtk/gtk.h>

#include "i18n.h"
#include "gui.h"

int main(int argc, char* argv[])
{
	/* Initialize gettext (gtk_init initializes the locale) */
	(void) textdomain(TEXTDOMAIN);
	(void) bind_textdomain_codeset(TEXTDOMAIN, "UTF-8");

	/* Get GTK ready, as appropriate.
	 * (We ignore passed parameters) */
	gtk_init(&argc, &argv);

	/* Create and show the window. */
	CreateMatchWindow();

	/* Start the main loop. And we're off! */
	gtk_main();

	return 0;
}

