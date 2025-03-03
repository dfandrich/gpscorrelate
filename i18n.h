/* i18n.h
 *
 * This file contains internationalization macros and includes
 */

/*
 * Copyright © 2001-2012 Lutz Müller <lutz@users.sourceforge.net> et. al.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef ENABLE_NLS
# include <libintl.h>
# undef _
# define _(String) gettext (String)
# ifdef gettext_noop
# define N_(String) gettext_noop (String)
# else
# define N_(String) (String)
# endif
#else
# define textdomain(String) (String)
# define gettext(String) (String)
# define ngettext(String1,String2,Count) (Count==1?String1:String2)
# define dgettext(Domain,Message) (Message)
# define dcgettext(Domain,Message,Type) (Message)
# define bindtextdomain(Domain,Directory) (Domain)
# define bind_textdomain_codeset(Domain,Codeset) (Domain)
# define _(String) (String)
# define N_(String) (String)
#endif

#define TEXTDOMAIN "gpscorrelate"
