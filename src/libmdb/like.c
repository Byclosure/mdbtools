/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>

//#define MDB_DEBUG_LIKE 1

int mdb_like_cmp(char *s, char *r)
{
int i, ret;

#if MDB_DEBUG_LIKE
				printf("comparing %s and %s\n", s, r);
#endif
	switch (r[0]) {
		case '\0':
			if (s[0]=='\0') {
				return 1;
			} else {
				return 0;
			}
		case '_':
			/* skip one character */
			return mdb_like_cmp(&s[1],&r[1]);
		case '%':
			/* skip any number of characters */
			/* the strlen(s)+1 is important so the next call can */
			/* if there are trailing characters */
			for(i=0;i<strlen(s)+1;i++) {
				if (mdb_like_cmp(&s[i],&r[1])) {
					return 1;
				}
			}
			return 0;
		default:
			for(i=0;i<strlen(r);i++) {
				if (r[i]=='_' || r[i]=='%') break;
			}
			if (strncmp(s,r,i)) {
				return 0;
			} else {
#if MDB_DEBUG_LIKE
				printf("at pos %d comparing %s and %s\n", i, &s[i], &r[i]);
#endif
				ret = mdb_like_cmp(&s[i],&r[i]);
#if MDB_DEBUG_LIKE
				printf("returning %d (%s and %s)\n", ret, &s[i], &r[i]);
#endif
				return ret;
			}
	}
}
