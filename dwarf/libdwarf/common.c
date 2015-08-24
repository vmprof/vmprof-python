/*
  Copyright (C) 2008-2010 SN Systems.  All Rights Reserved.
  Portions Copyright (C) 2008-2010 David Anderson.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement
  or the like.  Any license provided herein, whether implied or
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with
  other software, or any other product whatsoever.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, write the Free Software
  Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston MA 02110-1301,
  USA.

*/

#include "common.h"

void
print_version(const char * name)
{
#ifdef _DEBUG
    const char *acType = "Debug";
#else
    const char *acType = "Release";
#endif /* _DEBUG */

    char acVersion[60];
    snprintf(acVersion,sizeof(acVersion),"[%s %s %s]",
        __DATE__,__TIME__,acType);
    printf("%s %s\n",name,acVersion);
}

void
print_args(int argc, char *argv[])
{
    int index;
    printf("Arguments: ");
    for (index = 1; index < argc; ++index) {
        printf("%s ",argv[index]);
    }
    printf("\n");
}

void
print_usage_message(const char *options[])
{
    int index;
    for (index = 0; *options[index]; ++index) {
        printf("%s\n",options[index]);
    }
}
