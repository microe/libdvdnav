/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 */

#ifndef IFO_PRINT_H_INCLUDED
#define IFO_PRINT_H_INCLUDED

#include <inttypes.h>
#include <dvdnav/ifo_types.h> /*  Only for vm_cmd_t  */

void ifo_print(dvd_reader_t *dvd, int title);

#endif /* IFO_PRINT_H_INCLUDED */

/*
 * $Log$
 * Revision 1.4  2004/01/01 15:13:13  jcdutton
 * Put ifo_print.c and .h back in.
 *
 * Revision 1.2  2003/04/28 15:17:17  jcdutton
 * Update ifodump to work with new libdvdnav cvs, instead of needing libdvdread.
 *
 * Revision 1.1.1.1  2002/08/28 09:48:35  jcdutton
 * Initial import into CVS.
 *
 *
 *
 */

