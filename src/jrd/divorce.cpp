/*************	history	************ *
*	COMPONENT: JRD	MODULE: DIVORCE.C
*	generated by Marion V2.5     2/6/90
*	from dev              db	on 16-JUN-1995
*****************************************************************
*
*	19737	smistry	16-JUN-1995
*	Fix for the control terminal problem on SVR4 and SCO
*
*	19736	smistry	16-JUN-1995
*	A correction in the previous fix to tkae care of platform dependency
*
*	19735	smistry	16-JUN-1995
*	Fix for Control terminal reliquishing in SVR4 systems
*
*	14028	katz	15-NOV-1993
*	Define ANSI prototypes
*
*	13379	katz	16-OCT-1993
*	Move away from C language datatypes
*
*	0	katz	7-JUN-1993 
*	history begins
*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
*/


/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		divorce.c
 *	DESCRIPTION:	Divorce process from controlling terminal
 *
 * copyright (c) 1986 by Groton Database Systems, Inc.
 */

#include "firebird.h"
#include "../jrd/common.h"
#include "../jrd/divorce.h"

#ifdef _AIX
#include <sys/select.h>
#endif

#ifndef VMS
#ifdef WIN_NT
#include <io.h>
#endif
#endif	// !VMS


#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif


#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifndef NOFILE
#define NOFILE     20
#endif



void divorce_terminal(int mask)
{
/**************************************
 *
 *	d i v o r c e _ t e r m i n a l
 *
 **************************************
 *
 * Functional description
 *	Clean up everything in preparation to become an independent
 *	process.  Close all files except for marked by the input mask.
 *
 **************************************/
	int s, fid;

/* Close all files other than those explicitly requested to stay open */

	for (fid = 0; fid < NOFILE; fid++)
		if (!(mask & (1 << fid)))
			s = close(fid);

#ifdef SIGTTOU
	/* ignore all the teminal related signal if define */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
#endif

#ifdef TIOCNOTTY
/* Perform terminal divorce */
/* this is in case of BSD systems */

	fid = open("/dev/tty", 2);

	if (fid >= 0) {
		s = ioctl(fid, TIOCNOTTY, 0);
		s = close(fid);
	}
#endif


/* Finally, get out of the process group */

#if (!defined(VMS) && !defined(NETWARE_386) && !defined(WIN_NT))
	s = SETPGRP;
#endif
}
