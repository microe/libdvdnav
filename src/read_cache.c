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
#include <sys/time.h>
#include <time.h>

/*
#define DVDNAV_PROFILE
*/

/* Read-ahead cache structure. */
#if 0
/* #if _MULTITHREAD_ */

/* For the multithreaded cache, the cache is a ring buffer + writing
 * thread that continuously reads data into the buffer until it is
 * full or the 'upper-bound' has been reached.
 */

#define CACHE_BUFFER_SIZE 2048 /* Cache this number of blocks at a time */

struct read_cache_s {
  pthread_mutex_t cache_lock;
  pthread_t read_thread;
   
  /* Buffer */
  uint8_t   *buffer;
 
  /* Size of buffer */
  int32_t    size;
  /* block offset from sector start of buffer 'head' */
  uint32_t   pos;
  /* block offset from sector start of read point */
  uint32_t   read_point;
  /* block offset from buffer start to ring-boundary */
  uint32_t   start;
  
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

#define _MT_TRACE 1

#if _MT_TRACE
#define dprintf(fmt, args...) fprintf(stderr, "%s: "fmt,  ##__FUNCTION__##, ##args);
#else
#define dprintf(fmt, args...) /* Nowt */
#endif

#if 0
/* #if _MULTITHREAD_ */

void * read_cache_read_thread (void * this_gen) {
  int cont = 1;
  int32_t diff, start;
  uint32_t pos, size, startp, endp;
  uint32_t s,c;
  uint8_t *at;
  read_cache_t *self = (read_cache_t*)this_gen;

  while(cont) {
   
    pthread_mutex_lock(&self->cache_lock);
   
    if(self->size >= 0) {
      diff = self->read_point - self->pos;
      if(diff >= self->size/2) {
	dprintf("(II) Read thread -- ");

	startp = (self->start) % CACHE_BUFFER_SIZE;
	endp = abs((self->start + diff - 1) % CACHE_BUFFER_SIZE);
	dprintf("startp = %i, endp = %i -- ",startp, endp);
	
	pos = self->pos + diff;
	size = self->size - diff;
	start = (self->start + diff) % CACHE_BUFFER_SIZE;
	
	/* Fill remainder of buffer */

	if(startp > endp) {
	  s = pos + size; c = CACHE_BUFFER_SIZE - startp;
	  at = self->buffer + (startp * DVD_VIDEO_LB_LEN);
	  if(c > 0) {
	    dprintf("(1) Reading from %i to %i to %i ", s, s+c-1, startp);
	    pthread_mutex_unlock(&self->cache_lock);
	    DVDReadBlocks(self->dvd_self->file, s,c, at);
	    pthread_mutex_lock(&self->cache_lock);
	  }
	  
	  s = pos + size + c; c = CACHE_BUFFER_SIZE - size - c;
	  at = self->buffer;
	  if(c > 0) {
	    dprintf("(2) Reading from %i to %i to %i ", s, s+c-1, 0);
	    pthread_mutex_unlock(&self->cache_lock);
	    DVDReadBlocks(self->dvd_self->file, s,c, at);
	    pthread_mutex_lock(&self->cache_lock);
	  }
	} else {
	  s = pos + size; c = CACHE_BUFFER_SIZE - size;
	  at = self->buffer + (startp * DVD_VIDEO_LB_LEN);
	  if(c > 0) {
	    dprintf("(3) Reading from %i to %i to %i ", s, s+c-1, startp);
	    pthread_mutex_unlock(&self->cache_lock);
	    DVDReadBlocks(self->dvd_self->file, s,c, at);
	    pthread_mutex_lock(&self->cache_lock);
	  }
	}

	dprintf("\n");
	  
	self->pos = pos;
	self->start = start; self->size = CACHE_BUFFER_SIZE;
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
    me->read_point = 0;
    me->size = -1;

    /* Initialise the mutex */
    pthread_mutex_init(&me->cache_lock, NULL);

    if ((err = pthread_create (&me->read_thread,
			       NULL, read_cache_read_thread, me)) != 0) {
      dprintf("read_cache: can't create new thread (%s)\n",strerror(err));
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
  self->size = -1;
  self->start = 0;
  self->pos = 0;
  self->read_point = 0;
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
  dprintf("Requested pre-cache (%i -> +%i) : current state pos=%i, size=%i.\n", 
	 sector, block_count, self->pos, self->size);
  
  /* Are the contents of the buffer in any way relevant? */
  if((self->size > 0) && (sector >= self->pos) && (sector <= self->pos+self->size)) {
    dprintf("Contents relevant ... adjusting\n");
    self->read_point = sector;
  } else {
    /* Flush the cache as its not much use */
    dprintf("Contents irrelevent... flushing\n");
    self->size = 0;
    self->start = 0;
    self->pos = sector;
    self->read_point = sector;
  }
  
  pthread_mutex_unlock(&self->cache_lock);
}

/* This function will do the cache read once implemented */
int dvdnav_read_cache_block( read_cache_t *self, int sector, size_t block_count, uint8_t *buf) {
  int result, diff;
 
  if(!self)
   return 0;

  pthread_mutex_lock(&self->cache_lock);
  dprintf("Read from %i -> +%i (buffer pos=%i, read_point=%i, size=%i)... ", sector, block_count,
	 self->pos, self->read_point, self->size);
  if((self->size > 0) && (sector >= self->read_point) && 
     (sector + block_count <= self->pos + self->size)) {
    /* Hit */
    
    /* Drop any skipped blocks */
    diff = sector - self->read_point;
    if(diff > 0)
     self->read_point += diff;

    diff = self->read_point - self->pos;

    if(((self->start + diff) % CACHE_BUFFER_SIZE) + block_count <= CACHE_BUFFER_SIZE) {
      dprintf("************** Single read\n");
      memcpy(buf, self->buffer + (((self->start + diff) % CACHE_BUFFER_SIZE) * DVD_VIDEO_LB_LEN), 
	     block_count * DVD_VIDEO_LB_LEN);
      self->read_point += block_count;
      pthread_mutex_unlock(&self->cache_lock);

      return (int)block_count;
    } else {
      int32_t boundary = CACHE_BUFFER_SIZE - self->start;

      dprintf("************** Multiple read\n");
      memcpy(buf, self->buffer + (((self->start + diff) % CACHE_BUFFER_SIZE) * DVD_VIDEO_LB_LEN), 
	     boundary * DVD_VIDEO_LB_LEN);
      memcpy(buf + (boundary  * DVD_VIDEO_LB_LEN), self->buffer, 
	     (block_count-boundary) * DVD_VIDEO_LB_LEN);
      self->read_point += block_count;
      pthread_mutex_unlock(&self->cache_lock);

      return (int)block_count;      
    }
  } else {
    /* Miss */

    fprintf(stderr, "DVD read cache miss! (not bad but a performance hit) sector=%d\n", sector); 
    result = DVDReadBlocks( self->dvd_self->file, sector, block_count, buf);
    self->read_point = sector+block_count;
    if(self->read_point > self->pos + self->size) {
      /* Flush the cache as its not much use */
      dprintf("Contents irrelevent... flushing\n");
      self->size = 0;
      self->start = 0;
      self->pos = sector+block_count;
    }
    pthread_mutex_unlock(&self->cache_lock);
    usleep(300);
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

#ifdef DVDNAV_PROFILE
//#ifdef ARCH_X86
__inline__ unsigned long long int dvdnav_rdtsc()
{
  unsigned long long int x;
  __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
  return x;
}
//#endif
#endif

/* This function is called just after reading the NAV packet. */
void dvdnav_pre_cache_blocks(read_cache_t *self, int sector, size_t block_count) {
  int result;
#ifdef DVDNAV_PROFILE
  struct timeval tv1, tv2, tv3;
  unsigned long long p1, p2, p3;
#endif
 
  if(!self)
   return;
  
  if(!self->dvd_self->use_read_ahead) {
    self->cache_valid = 0;
    self->cache_start_sector = -1;
    return;
  }
  /* We start with a sensible figure for the first malloc of 500 blocks.
   * Some DVDs I have seen venture to 450 blocks.
   * This is so that fewer realloc's happen if at all.
   */ 
  if (self->cache_buffer) {
    if(block_count > self->cache_malloc_size) {
      self->cache_buffer = realloc(self->cache_buffer, block_count * DVD_VIDEO_LB_LEN);
      dprintf("libdvdnav:read_cache:pre_cache DVD read realloc happened\n");
      self->cache_malloc_size = block_count;
    } 
  } else {
    self->cache_buffer = malloc((block_count > 500 ? block_count : 500 )* DVD_VIDEO_LB_LEN);
    self->cache_malloc_size = (block_count > 500 ? block_count : 500 );
    dprintf("libdvdnav:read_cache:pre_cache DVD read malloc %d\n", (block_count > 500 ? block_count : 500 )); 
  }
  self->cache_start_sector = sector;
  self->cache_block_count = block_count;
#ifdef DVDNAV_PROFILE
  gettimeofday(&tv1, NULL);
  p1 = dvdnav_rdtsc();
#endif
  result = DVDReadBlocks( self->dvd_self->file, sector, block_count, self->cache_buffer);
#ifdef DVDNAV_PROFILE
  p2 = dvdnav_rdtsc();
  gettimeofday(&tv2, NULL);
  timersub(&tv2, &tv1, &tv3);
  dprintf("libdvdnav:read_cache:pre_cache DVD read %ld us, profile = %lld, block_count = %d\n", tv3.tv_usec, p2-p1, block_count); 
#endif
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
  }
  /* Disable dprintf if read cache is disabled. */
  //if(self->dvd_self->use_read_ahead) {
  //  dprintf("DVD read cache miss! sector=%d, start=%d, end=%d\n",
  //           sector, self->cache_start_sector, self->cache_block_count + self->cache_start_sector); 
  //}
  result = DVDReadBlocks( self->dvd_self->file, sector, block_count, buf);
  return result;
}

#endif

