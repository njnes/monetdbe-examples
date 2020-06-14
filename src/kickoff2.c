/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * (c) 2020- MonetDB Solutions B.V.
 * This version of kickoff has been written to exercise the more enhanced result structure
 * references as a monetdbe_table structure
 * typedef struct {
   monetdb_cnt nrows;
   int ncols;
  int affectedrows;
  void *header[];  // the result set header
  void *data[];
} monetdb_table;
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
	monetdbe_database  db = NULL;		// a reference
	monetdbe_connection conn = NULL; 	// a reference
	monetdbe_table result = NULL;		// a reference

	// first argument is a string for the db directory or NULL for in-memory mode
	if ((err = monetdbe_startup()) != NULL)
		error("Could not initialize the database");

	if ((err = monetdbe_open(NULL, &db)))
		error(err)

	if ((err = monetdbe_connect(&conn)))
		error(err)

	// ignore errors
	monetdbe_query(conn, "CREATE TABLE test (x integer, y string)", NULL);
	monetdbe_query(conn, "INSERT INTO test VALUES (42, 'Hello'), (NULL, 'World')", NULL);

	monetdbe_query(conn, "SELECT x, y FROM test; ", &result);

	if( result->error)
		error(result->error);

	fprintf(stdout, "Query result with %zu cols and %"PRId64" rows\n", result->ncols, result->nrows);
	for (int64_t r = 0; r < result->nrows; r++) {
		for (size_t c = 0; c < result->ncols; c++) {
			switch (result->header[c]->type) {
				case monetdbe_int32_t: {
					if (result->data[c][r] == col->null_value) 
						printf("NULL");
					else
						printf("%d", result->data[c][r]);
					break;
				}
				case monetdbe_str: {
					if ( monetdbe_is_null_str(result->data[x][r]))
						printf("NULL");
					else
						printf("%s", (char*) result->data[c][r]);
					break;
				}
				default: {
					printf("UNKNOWN");
				}
			}

			if (c + 1 < result->ncols) {
				printf(", ");
			}
		}
		printf("\n");
	}

	if ((err = monetdbe_cleanup_result(result)) != NULL)
		error(err)
	if ((err = monetdbe_close(conn)) != NULL)
		error(err)
	if ((err = monetdbe_disconnect(db)) != NULL)
		error(err)
	if ((err = monetdbe_shutdown()) != NULL)
		error(err)
	return 0;
}