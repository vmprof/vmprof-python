/* $NetBSD: getopt.c,v 1.1 2009/03/22 22:33:13 joerg Exp $*/
/*  Modified by David Anderson to work with GNU/Linux and freebsd.
    Added {} for clarity.
    Switched to standard dwarfdump formatting.
    Treatment of : modified so that :: gets optarg NULL
    if space follows the letter
    (the optarg is set to null).
    renamed to make it clear this is a private version.
*/
/*
* Copyright (c) 1987, 1993, 1994
* The Regents of the University of California.  All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. Neither the name of the University nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

/*  This does not presently handle the option string
    leading + or leading - features. Such are not used
    by by libdwarfdump.  Nor does it understand the
    GNU Env var POSIXLY_CORRECT .
    It does know of the leading ":" in the option string.
    See BADCH below.
    */

#include <stdio.h>
#include <stdlib.h> /* For exit() */
#include <string.h> /* For strchr */

#define STRIP_OFF_CONSTNESS(a)  ((void *)(size_t)(const void *)(a))

int opterr = 1,     /* if error message should be printed */
    optind = 1,    /* index into parent argv vector */
    optopt,        /* character checked for validity */
    optreset;      /* reset getopt */
char *optarg;      /* argument associated with option */

#define BADCH   (int)'?'
#define BADARG  (int)':'
#define EMSG    ""

/*  Use for testing dwgetopt only.
    Not a standard function. */
void
dwgetoptresetfortestingonly()
{
   opterr   = 1;
   optind   = 1;
   optopt   = 0;
   optreset = 0;
   optarg   = 0;
}

/*
    * getopt --
    * Parse argc/argv argument vector.
    * a: means
    *     -afoo
    *     -a foo
    *     and 'foo' is returned in optarg
    *  b:: means
    *     -b
    *        and optarg is null
    *     -bother
    *        and optarg is 'other'
    */
int
dwgetopt(int nargc, char * const nargv[], const char *ostr)
{
    static const char *place = EMSG;/* option letter processing */
    char *oli;                      /* option letter list index */

#if 0
    _DIAGASSERT(nargv != NULL);
    _DIAGASSERT(ostr != NULL);
#endif

    if (optreset || *place == 0) { /* update scanning pointer */
        optreset = 0;
        place = nargv[optind];
        if (optind >= nargc || *place++ != '-') {
            /* Argument is absent or is not an option */
            place = EMSG;
            return (-1);
        }
        optopt = *place++;
        if (optopt == '-' && *place == 0) {
            /* "--" => end of options */
            ++optind;
            place = EMSG;
            return (-1);
        }
        if (optopt == 0) {
            /* Solitary '-', treat as a '-' option
                if the program (eg su) is looking for it. */
            place = EMSG;
            if (strchr(ostr, '-') == NULL) {
                return -1;
            }
            optopt = '-';
        }
    } else {
        optopt = *place++;
    }
    /* See if option letter is one the caller wanted... */
    if (optopt == ':' || (oli = strchr(ostr, optopt)) == NULL) {
        if (*place == 0) {
            ++optind;
        }
        if (opterr && *ostr != ':') {
            (void)fprintf(stderr,
                "%s: invalid option -- '%c'\n",
                nargv[0]?nargv[0]:"",
                optopt);
        }
        return (BADCH);
    }

    /* Does this option need an argument? */
    if (oli[1] != ':') {
        /* don't need argument */
        optarg = NULL;
        if (*place == 0) {
            ++optind;
        }
    } else {
        int reqnextarg = 1;
        if (oli[1] && (oli[2] == ':')) {
            /* Pair of :: means special treatment of optarg */
            reqnextarg = 0;
        }
        /* Option-argument is either the rest of this argument or the
        entire next argument. */
        if (*place ) {
            /* Whether : or :: */
            optarg = STRIP_OFF_CONSTNESS(place);
        } else if (reqnextarg) {
            /* ! *place */
            if (nargc > (++optind)) {
                optarg = nargv[optind];
            } else {
                place=EMSG;
                /*  Next arg required, but is missing */
                if (*ostr == ':') {
                    /* Leading : in ostr calls for BADARG return. */
                    return (BADARG);
                }
                if (opterr) {
                    (void)fprintf(stderr,
                        "%s: option requires an argument. -- '%c'\n",
                        nargv[0]?nargv[0]:"",
                        optopt);
                }
                return (BADCH);
            }
        } else {
            /* ! *place */
            /* The key part of :: treatment. */
            optarg = NULL;
        }
        place = EMSG;
        ++optind;
    }
    return (optopt);  /* return option letter */
}
