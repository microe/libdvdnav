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

#ifndef VMCMD_H_INCLUDED
#define VMCMD_H_INCLUDED

#define MSG_OUT stdout

#include <inttypes.h>

#ifndef _MSC_VER
#include <dvdnav/ifo_types.h> /*  Only for vm_cmd_t  */
#else
#include <ifo_types.h> /*  Only for vm_cmd_t  */
#endif

void vm_print_mnemonic(vm_cmd_t *command);
void vm_print_cmd(int row, vm_cmd_t *command);

#endif /* VMCMD_H_INCLUDED */

/*
 * $Log$
 * Revision 1.8  2003/05/05 00:25:12  tchamp
 * Changed the linkage for msvc
 *
 * Revision 1.4  2003/04/28 15:17:18  jcdutton
 * Update ifodump to work with new libdvdnav cvs, instead of needing libdvdread.
 *
 * Revision 1.3  2003/04/05 13:03:49  jcdutton
 * Small updates.
 *
 * Revision 1.2  2003/04/05 12:45:48  jcdutton
 * Use MSG_OUT instead of stdout.
 *
 * Revision 1.1.1.1  2002/08/28 09:48:35  jcdutton
 * Initial import into CVS.
 *
 *
 *
 */

