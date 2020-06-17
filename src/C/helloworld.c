/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) 2020- MonetDB Solutions B.V.
 *
 * This trivial program can be used to check if all the basic ingredients
 * for using MonetDBe has been available and accessible.
 *
 * For an explanation of the command arguments see ...
 */

#include "monetdbe.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#define error(msg) {fprintf(stderr, "Failure: %s\n", msg); return -1;}

int
main(void)
{
	char* err = NULL;
	monetdbe_database mdbe = NULL;

	// second argument is a string for the db directory or NULL for in-memory mode
	if ((err = monetdbe_open(&mdbe, NULL, NULL)) != NULL)
		error(err)

	printf("hello world, we have a lift off\n MonetDBe has been started\n");

	if ((err = monetdbe_close(mdbe)) != NULL)
		error(err)

	printf("hello world, we savely returned\n");
	return 0;
}
