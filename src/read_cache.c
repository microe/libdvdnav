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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "read_cache.h"

/* This function MUST be called whenever self->file changes. */
void dvdnav_read_cache_clear(dvdnav_t *self) {
  if(!self)
   return;
  
  self->cache_start_sector = -1;
  self->cache_valid = 0;
}
/* This function is called just after reading the NAV packet. */
void dvdnav_pre_cache_blocks(dvdnav_t *self, int sector, size_t block_count) {
  int result;
 
  if(!self)
   return;
  
  if(!self->use_read_ahead) {
    self->cache_valid = 0;
    self->cache_start_sector = -1;
    return;
  }
  
  if (self->cache_buffer) {
    if( block_count > self->cache_malloc_size) {
      self->cache_buffer = realloc(self->cache_buffer, block_count * DVD_VIDEO_LB_LEN);
      self->cache_malloc_size = block_count;
    } 
  } else {
    self->cache_buffer = malloc(block_count * DVD_VIDEO_LB_LEN);
    self->cache_malloc_size = block_count;
  }
  self->cache_start_sector = sector;
  self->cache_block_count = block_count;
  result = DVDReadBlocks( self->file, sector, block_count, self->cache_buffer);
  self->cache_valid = 1;
}

/* This function will do the cache read once implemented */
int dvdnav_read_cache_block( dvdnav_t *self, int sector, size_t block_count, uint8_t *buf) {
  int result;
 
  if(!self)
   return 0;

  if(self->cache_valid && self->use_read_ahead) {
    if (self->cache_start_sector != -1 ) {
      if ((sector >= self->cache_start_sector) && 
	  (sector < self->cache_start_sector + self->cache_block_count)) {
	memcpy(buf, self->cache_buffer + ((off_t)((off_t)sector - (off_t)self->cache_start_sector) * DVD_VIDEO_LB_LEN), DVD_VIDEO_LB_LEN);
	return DVD_VIDEO_LB_LEN;
      }
    }
  } else {
    result = DVDReadBlocks( self->file, sector, block_count, buf);
    return result;
  }
  
  printf("DVD read cache miss! sector=%d, start=%d\n", sector, self->cache_start_sector); 
  result = DVDReadBlocks( self->file, sector, block_count, buf);
  return result;
}


