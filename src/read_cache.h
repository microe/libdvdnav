/* 
 * Copyright (C) 2000 Rich Wareham <richwareham@users.sourceforge.net>
 * 
 * This file is part of libdvdnav, a DVD navigation library.
 * 
 * libdvdnav is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * libdvdnav is distributed in the hope that it will be useful,
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

#ifndef __DVDNAV_READ_CACHE_H
#define __DVDNAV_READ_CACHE_H

#include "dvdnav_internal.h"

/* This function MUST be called whenever self->file changes. */
void dvdnav_read_cache_clear(dvdnav_t *self);
/* This function is called just after reading the NAV packet. */
void dvdnav_pre_cache_blocks(dvdnav_t *self, int sector, size_t block_count);
/* This function will do the cache read */
int dvdnav_read_cache_block(dvdnav_t *self, int sector, size_t block_count, uint8_t *buf);

#endif /* __DVDNAV_READ_CACHE_H */
