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
#include <pthread.h>

/* Read-ahead cache structure. */
#if _MULTITHREAD_

/* For the multithreaded cache, the cache is a ring buffer + writing
 * thread that continuously reads data into the buffer until it is
 * full or the 'upper-bound' has been reached.
 */

#define CACHE_BUFFER_SIZE 1024 /* Cache this number of blocks at a time */

struct read_cache_s {
  pthread_mutex_t cache_lock;
  pthread_t read_thread;
   
  /* Buffer */
  uint8_t   *buffer;
  
  /* Where we are currently 'reading' from (blocks) */
  int32_t    pos;
  /* Location of 'start-point' in buffer */
  int32_t    start;
  /* How much is in buffer? */
  int        size;
  
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

#if _MULTITHREAD_

void * read_cache_read_thread (void * this_gen) {
  int cont = 1;
  int32_t diff, start;
  uint32_t s,c;
  uint8_t *at;
  read_cache_t *self = (read_cache_t*)this_gen;

  while(cont) {
   
    pthread_mutex_lock(&self->cache_lock);
    if((self->size >= 0) && (self->size < CACHE_BUFFER_SIZE/3)) {
      printf("Rad thread -- current state pos=%i, size=%i\n", self->pos, self->size);
      /* Attempt to fill buffer */
      diff = CACHE_BUFFER_SIZE - self->size;
      if(diff > 0) {
	start = (self->start + self->size) % CACHE_BUFFER_SIZE;

	c = 0; s = self->pos + self->size;
	if(start != 0) {
	  s = self->pos + self->size; c = CACHE_BUFFER_SIZE - start;
	  at = self->buffer + (start * DVD_VIDEO_LB_LEN);
	  pthread_mutex_unlock(&self->cache_lock);
	  DVDReadBlocks( self->dvd_self->file, s,c,at);
	  pthread_mutex_lock(&self->cache_lock);
	}
	
	s += c; c = diff - c;
	at = self->buffer;
	pthread_mutex_unlock(&self->cache_lock);
	DVDReadBlocks( self->dvd_self->file, s,c,at);
	pthread_mutex_lock(&self->cache_lock);

	// end = (self->start + self->size + diff) % CACHE_BUFFER_SIZE;
	/*if(end > start) {
	  printf("Reading %i from sector %i\n",diff, self->pos+self->size);
	  pthread_mutex_unlock(&self->cache_lock);
	  DVDReadBlocks( self->dvd_self->file, self->pos + self->size, diff, 
			 self->buffer + (start * DVD_VIDEO_LB_LEN));
	  pthread_mutex_lock(&self->cache_lock);
	} else {*/
	/*
	  if(start != CACHE_BUFFER_SIZE-1) {
	    printf(" -- Reading %i from %i to %i ", CACHE_BUFFER_SIZE - start,
		   self->pos + self->size, start);
	    s = self->pos + self->size; c = CACHE_BUFFER_SIZE - start;
	    at = self->buffer + (start * DVD_VIDEO_LB_LEN);
	    pthread_mutex_unlock(&self->cache_lock);
	    DVDReadBlocks( self->dvd_self->file, s,c,at);
	    pthread_mutex_lock(&self->cache_lock);
	  }
	  if(end != 0) {
	    printf(" -- Reading %i from %i to %i ", diff - CACHE_BUFFER_SIZE + start,
		   self->pos + self->size + CACHE_BUFFER_SIZE - start,
		   0);
	    s = self->pos + self->size + CACHE_BUFFER_SIZE - start;
	    c = diff - CACHE_BUFFER_SIZE + start; at = self->buffer;
	    pthread_mutex_unlock(&self->cache_lock);
	    DVDReadBlocks( self->dvd_self->file, s, c, at);
	    pthread_mutex_lock(&self->cache_lock);
	  }
	  printf("::\n");
	  */
	// }

	self->size += diff;
      }
    }
   
    pthread_mutex_unlock(&self->cache_lock);
    cont = (self->buffer != NULL);
    usleep(100);
  }

  return NULL;
}
					   
read_cache_t *dvdnav_read_cache_new(dvdnav_t* dvd_self) {
  read_cache_t *me;

  me = (read_cache_t*)malloc(sizeof(struct read_cache_s));

  if(me) {
    int err;
    
    me->dvd_self = dvd_self;
    me->buffer = (uint8_t*)malloc(CACHE_BUFFER_SIZE * DVD_VIDEO_LB_LEN);
    me->start = 0;
    me->pos = 0;
    me->size = -1;

    /* Initialise the mutex */
    pthread_mutex_init(&me->cache_lock, NULL);

    if ((err = pthread_create (&me->read_thread,
			       NULL, read_cache_read_thread, me)) != 0) {
      fprintf(stderr,"read_cache: can't create new thread (%s)\n",strerror(err));
    }
  }
  
  return me;
}

void dvdnav_read_cache_free(read_cache_t* self) {
  pthread_mutex_lock(&self->cache_lock);
		   
  if(self->buffer) {
    free(self->buffer);
    self->buffer = NULL;
    self->size = -2;
  }

  pthread_mutex_unlock(&self->cache_lock);

  pthread_join(self->read_thread, NULL);
  
  pthread_mutex_destroy(&self->cache_lock);

  free(self);
}

/* This function MUST be called whenever self->file changes. */
void dvdnav_read_cache_clear(read_cache_t *self) {
  if(!self)
   return;
  
  pthread_mutex_lock(&self->cache_lock);
  self->size = 0;
  self->start = 0;
  self->pos = 0;
  pthread_mutex_unlock(&self->cache_lock);
}

/* This function is called just after reading the NAV packet. */
void dvdnav_pre_cache_blocks(read_cache_t *self, int sector, size_t block_count) {
  if(!self)
   return;
  
  if(!self->dvd_self->use_read_ahead) {
    return;
  }
 
  pthread_mutex_lock(&self->cache_lock);
  printf("Requested pre-cache.\n");
  
  /* Are the contents of the buffer in any way relevant? */
  if((sector < self->pos+self->size) && (sector >= self->pos)) {
    int32_t diff = sector - self->pos;

    printf("Contents relevant ... adjusting by %i\n", diff);
    self->size -= diff;
    self->start = (self->start + diff) % CACHE_BUFFER_SIZE;
  } else {
    /* Flush the cache as its not much use */
    pthread_mutex_unlock(&self->cache_lock);
    printf("Contents irrelevent... flushing\n");
    dvdnav_read_cache_clear(self);
    pthread_mutex_lock(&self->cache_lock);
  }
  
  self->pos = sector;

  pthread_mutex_unlock(&self->cache_lock);
}

/* This function will do the cache read once implemented */
int dvdnav_read_cache_block( read_cache_t *self, int sector, size_t block_count, uint8_t *buf) {
  int result;
 
  if(!self)
   return 0;

  pthread_mutex_lock(&self->cache_lock);
  if((self->size > 0) && (sector >= self->pos) && (sector + block_count <= self->pos + self->size)) {
    /* Hit */

    //printf("Read from %i -> +%i (buffer pos=%i, size=%i)... ", sector, block_count,
	//   self->pos, self->size);
    
    /* Drop any skipped blocks */
     {
      int diff = sector - self->pos;

      self->pos += diff;
      self->size -= diff;
      self->start = (self->start + diff) % CACHE_BUFFER_SIZE;
     }

    if(self->start + block_count <= CACHE_BUFFER_SIZE) {
      // printf("Single read\n");
      memcpy(buf, self->buffer + ((self->start) * DVD_VIDEO_LB_LEN), block_count * DVD_VIDEO_LB_LEN);
      self->start = (self->start + block_count) % CACHE_BUFFER_SIZE;
      self->pos += block_count;
      self->size --;
      pthread_mutex_unlock(&self->cache_lock);

      return (int)block_count;
    } else {
      int32_t boundary = CACHE_BUFFER_SIZE - self->start - 1;

      printf("************** Multiple read\n");
      memcpy(buf, self->buffer + ((self->start) * DVD_VIDEO_LB_LEN), boundary  * DVD_VIDEO_LB_LEN);
      memcpy(buf + (boundary  * DVD_VIDEO_LB_LEN), self->buffer, (block_count-boundary) * DVD_VIDEO_LB_LEN);
      self->start = (self->start + block_count) % CACHE_BUFFER_SIZE;
      self->pos += block_count;
      self->size --;
      pthread_mutex_unlock(&self->cache_lock);

      return (int)block_count;      
    }
  } else {
    /* Miss */

    fprintf(stderr,"DVD read cache miss! sector=%d\n", sector); 
    result = DVDReadBlocks( self->dvd_self->file, sector, block_count, buf);
    self->pos = sector + block_count;
    usleep(200);
    pthread_mutex_unlock(&self->cache_lock);
    return result;
  }
  
  /* Should never get here */
  return 0;
}

#else

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


#endif
