/*
  Copyright (C) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement
  or the like.  Any license provided herein, whether implied or
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with
  other software, or any other product whatsoever.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 51
  Franklin Street - Fifth Floor, Boston MA 02110-1301, USA.

   makename.c
   $Revision: 1.4 $
   $Date: 2005/11/08 21:48:42 $

   This used to be elaborate stuff.
   Now it is trivial, as duplicating names is
   unimportant in dwarfdump (in general).

   And in fact, this is only called for attributes and
   tags etc whose true name is unknown. Not for
   any normal case.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "makename.h"

#ifdef WIN32
#pragma warning(disable:4996)    /* Warning when migrated to VS2010 */
#endif /* WIN32 */

char *
makename(const char *s)
{
    char *newstr = 0;

    if (!s) {
        return "";
    }

    newstr = (char *)strdup(s);
    if (newstr == 0) {
        fprintf(stderr, "Out of memory mallocing %d bytes\n",
            (int) strlen(s));
        exit(1);
    }
    return newstr;
}
