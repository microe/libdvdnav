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

#include "dvdnav.h"
#include "read_cache.h"

/* Read-ahead cache structure. */
#if _MULTITHREAD_
struct read_cache_s {

  /* Bounds on read ahead buffer */
  off_t      low_bound;
  off_t      high_bound;

  /* Where we are currently 'reading' from */
  
  /* Bit of strange cross-linking going on here :) -- Gotta love C :) */
  dvdnav_t    *dvd_self;
};

#else
struct read_cache_s {
  /* Read-ahead cache. */
  uint8_t      *cache_buffer;
  int32_t      cache_start_sector; /* -1 means cache invalid */
  size_t       cache_block_count;
  size_t       cache_malloc_size;
  int          cache_valid;

  /* Bit of strange cross-linking going on here :) -- Gotta love C :) */
  dvdnav_t    *dvd_self;
};
#endif

read_cache_t *dvdnav_read_cache_new(dvdnav_t* dvd_self) {
  read_cache_t *me;

  me = (read_cache_t*)malloc(sizeof(struct read_cache_s));

  if(me) {
    me->dvd_self = dvd_self;

    dvdnav_read_cache_clear(me);
    me->cache_buffer = NULL;
  }
  
  /* this->cache_start_sector = -1;
  this->cache_block_count = 0;
  this->cache_valid = 0; */

  return me;
}

void dvdnav_read_cache_free(read_cache_t* self) {
  if(self->cache_buffer) {
    free(self->cache_buffer);
    self->cache_buffer = NULL;
  }

  free(self);
}

/* This function MUST be called whenever self->file changes. */
void dvdnav_read_cache_clear(read_cache_t *self) {
  if(!self)
   return;
  
  self->cache_start_sector = -1;
  self->cache_valid = 0;
}

/* This function is called just after reading the NAV packet. */
void dvdnav_pre_cache_blocks(read_cache_t *self, int sector, size_t block_count) {
  int result;
 
  if(!self)
   return;
  
  if(!self->dvd_self->use_read_ahead) {
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
  result = DVDReadBlocks( self->dvd_self->file, sector, block_count, self->cache_buffer);
  self->cache_valid = 1;
}

/* This function will do the cache read once implemented */
int dvdnav_read_cache_block( read_cache_t *self, int sector, size_t block_count, uint8_t *buf) {
  int result;
 
  if(!self)
   return 0;

  if(self->cache_valid && self->dvd_self->use_read_ahead) {
    if (self->cache_start_sector != -1 ) {
      if ((sector >= self->cache_start_sector) && 
	  (sector < self->cache_start_sector + self->cache_block_count)) {
	memcpy(buf, self->cache_buffer + ((off_t)((off_t)sector - (off_t)self->cache_start_sector) * DVD_VIDEO_LB_LEN), DVD_VIDEO_LB_LEN);
	return DVD_VIDEO_LB_LEN;
      }
    }
  } else {
    result = DVDReadBlocks( self->dvd_self->file, sector, block_count, buf);
    return result;
  }
  
  fprintf(stderr,"DVD read cache miss! sector=%d, start=%d\n", sector, self->cache_start_sector); 
  result = DVDReadBlocks( self->dvd_self->file, sector, block_count, buf);
  return result;
}


