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


#define DVDNAV_PROFILE


/* Read-ahead cache structure. */
#if _MULTITHREAD_

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

#define READ_CACHE_CHUNKS 10

/* all cache chunks must be memory aligned to allow use of raw devices */
#define ALIGNMENT 2048

#define READ_AHEAD_SIZE_MIN 4
#define READ_AHEAD_SIZE_MAX 512

typedef struct read_cache_chunk_s {
  uint8_t     *cache_buffer;
  uint8_t     *cache_buffer_base;  /* used in malloc and free for alignment */
  int32_t      cache_start_sector; /* -1 means cache invalid */
  int32_t      cache_read_count;   /* this many sectors are already read */
  size_t       cache_block_count;  /* this many sectors will go in this chunk */
  size_t       cache_malloc_size;
  int          cache_valid;
  int          usage_count;  /* counts how many buffers where issued from this chunk */
} read_cache_chunk_t;

struct read_cache_s {
  read_cache_chunk_t  chunk[READ_CACHE_CHUNKS];
  int                 current;
  int                 freeing;  /* is set to one when we are about to dispose the cache */
  int                 read_ahead_size;
  int                 read_ahead_incr;
  int                 last_sector;
  pthread_mutex_t     lock;

  /* Bit of strange cross-linking going on here :) -- Gotta love C :) */
  dvdnav_t           *dvd_self;
};
#endif

/*
#define READ_CACHE_TRACE 0
*/

#ifdef __GNUC__
# if READ_CACHE_TRACE
#  define dprintf(fmt, args...) fprintf(MSG_OUT, "libdvdnav: %s: "fmt,  __func__ , ## arg
# else
#  define dprintf(fmt, args...) /* Nowt */
# endif
#else
# if READ_CACHE_TRACE
#  define dprintf(fmt, ...) fprintf(MSG_OUT, "libdvdnav: %s: "fmt,  __func__ , __VA_ARGS_
# else
#  define dprintf(fmt, ...) /* Nowt */
# endif
#endif

#if _MULTITHREAD_

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
  dvdnav_t *tmp;

  pthread_mutex_lock(&self->cache_lock);

  if(self->buffer) {
    free(self->buffer);
    self->buffer = NULL;
    self->size = -2;
  }

  pthread_mutex_unlock(&self->cache_lock);

  pthread_join(self->read_thread, NULL);

  pthread_mutex_destroy(&self->cache_lock);

  tmp = self->dvd_self;
  free(self);

  /* We free the main structure, too, because we have no buffers out there. */
  free(tmp);
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
int dvdnav_read_cache_block( read_cache_t *self, int sector, size_t block_count, uint8_t **buf) {
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
      memcpy(*buf, self->buffer + (((self->start + diff) % CACHE_BUFFER_SIZE) * DVD_VIDEO_LB_LEN),
	     block_count * DVD_VIDEO_LB_LEN);
      self->read_point += block_count;
      pthread_mutex_unlock(&self->cache_lock);

      return (int)block_count;
    } else {
      int32_t boundary = CACHE_BUFFER_SIZE - self->start;

      dprintf("************** Multiple read\n");
      memcpy(*buf, self->buffer + (((self->start + diff) % CACHE_BUFFER_SIZE) * DVD_VIDEO_LB_LEN),
	     boundary * DVD_VIDEO_LB_LEN);
      memcpy(*buf + (boundary  * DVD_VIDEO_LB_LEN), self->buffer,
	     (block_count-boundary) * DVD_VIDEO_LB_LEN);
      self->read_point += block_count;
      pthread_mutex_unlock(&self->cache_lock);

      return (int)block_count;
    }
  } else {
    /* Miss */

    fprintf(MSG_OUT, "libdvdnav: DVD read cache miss! (not bad but a performance hit) sector=%d\n", sector);
    result = DVDReadBlocks( self->dvd_self->file, sector, block_count, *buf);
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

dvdnav_status_t dvdnav_free_cache_block(dvdnav_t *self, unsigned char *buf) {
  return DVDNAV_STATUS_OK;
}

#else

read_cache_t *dvdnav_read_cache_new(dvdnav_t* dvd_self) {
  read_cache_t *self;
  int i;

  self = (read_cache_t *)malloc(sizeof(read_cache_t));

  if(self) {
    self->current = 0;
    self->freeing = 0;
    self->dvd_self = dvd_self;
    self->last_sector = 0;
    self->read_ahead_size = READ_AHEAD_SIZE_MIN;
    self->read_ahead_incr = 0;
    pthread_mutex_init(&self->lock, NULL);
    dvdnav_read_cache_clear(self);
    for (i = 0; i < READ_CACHE_CHUNKS; i++) {
      self->chunk[i].cache_buffer = NULL;
      self->chunk[i].usage_count = 0;
    }
  }

  return self;
}

void dvdnav_read_cache_free(read_cache_t* self) {
  dvdnav_t *tmp;
  int i;

  pthread_mutex_lock(&self->lock);
  self->freeing = 1;
  for (i = 0; i < READ_CACHE_CHUNKS; i++)
    if (self->chunk[i].cache_buffer && self->chunk[i].usage_count == 0) {
      free(self->chunk[i].cache_buffer_base);
      self->chunk[i].cache_buffer = NULL;
    }
  pthread_mutex_unlock(&self->lock);

  for (i = 0; i < READ_CACHE_CHUNKS; i++)
    if (self->chunk[i].cache_buffer) return;

  /* all buffers returned, free everything */
  tmp = self->dvd_self;
  pthread_mutex_destroy(&self->lock);
  free(self);
  free(tmp);
}

/* This function MUST be called whenever self->file changes. */
void dvdnav_read_cache_clear(read_cache_t *self) {
  int i;

  if(!self)
   return;

  pthread_mutex_lock(&self->lock);
  for (i = 0; i < READ_CACHE_CHUNKS; i++)
    self->chunk[i].cache_valid = 0;
  pthread_mutex_unlock(&self->lock);
}

/* This function is called just after reading the NAV packet. */
void dvdnav_pre_cache_blocks(read_cache_t *self, int sector, size_t block_count) {
  int i, use;

  if(!self)
    return;

  if(!self->dvd_self->use_read_ahead)
    return;

  pthread_mutex_lock(&self->lock);

  /* find a free cache chunk that best fits the required size */
  use = -1;
  for (i = 0; i < READ_CACHE_CHUNKS; i++)
    if (self->chunk[i].usage_count == 0 && self->chunk[i].cache_buffer &&
        self->chunk[i].cache_malloc_size >= block_count &&
        (use == -1 || self->chunk[use].cache_malloc_size > self->chunk[i].cache_malloc_size))
      use = i;

  if (use == -1) {
    /* we haven't found a cache chunk, so we try to reallocate an existing one */
    for (i = 0; i < READ_CACHE_CHUNKS; i++)
      if (self->chunk[i].usage_count == 0 && self->chunk[i].cache_buffer &&
          (use == -1 || self->chunk[use].cache_malloc_size < self->chunk[i].cache_malloc_size))
        use = i;
    if (use >= 0) {
      self->chunk[use].cache_buffer_base = realloc(self->chunk[use].cache_buffer_base,
        block_count * DVD_VIDEO_LB_LEN + ALIGNMENT);
      self->chunk[use].cache_buffer =
        (uint8_t *)(((int)self->chunk[use].cache_buffer_base & ~(ALIGNMENT - 1)) + ALIGNMENT);
      dprintf("pre_cache DVD read realloc happened\n");
      self->chunk[use].cache_malloc_size = block_count;
    } else {
      /* we still haven't found a cache chunk, let's allocate a new one */
      for (i = 0; i < READ_CACHE_CHUNKS; i++)
        if (!self->chunk[i].cache_buffer) {
	  use = i;
	  break;
	}
      if (use >= 0) {
        /* We start with a sensible figure for the first malloc of 500 blocks.
         * Some DVDs I have seen venture to 450 blocks.
         * This is so that fewer realloc's happen if at all.
         */
	self->chunk[i].cache_buffer_base =
	  malloc((block_count > 500 ? block_count : 500) * DVD_VIDEO_LB_LEN + ALIGNMENT);
	self->chunk[i].cache_buffer =
	  (uint8_t *)(((int)self->chunk[i].cache_buffer_base & ~(ALIGNMENT - 1)) + ALIGNMENT);
	self->chunk[i].cache_malloc_size = block_count > 500 ? block_count : 500;
	dprintf("pre_cache DVD read malloc %d blocks\n",
	  (block_count > 500 ? block_count : 500 ));
      }
    }
  }

  if (use >= 0) {
    self->chunk[use].cache_start_sector = sector;
    self->chunk[use].cache_block_count = block_count;
    self->chunk[use].cache_read_count = 0;
    self->chunk[use].cache_valid = 1;
    self->current = use;
  } else {
    dprintf("pre_caching was impossible, no cache chunk available\n");
  }
  pthread_mutex_unlock(&self->lock);
}

int dvdnav_read_cache_block(read_cache_t *self, int sector, size_t block_count, uint8_t **buf) {
  int i, use;
  int start;
  int size;
  int incr;
  uint8_t *read_ahead_buf;
  int32_t res;

  if(!self)
    return 0;

  use = -1;

  if(self->dvd_self->use_read_ahead) {
    /* first check, if sector is in current chunk */
    read_cache_chunk_t cur = self->chunk[self->current];
    if (cur.cache_valid && sector >= cur.cache_start_sector &&
        sector <= (cur.cache_start_sector + cur.cache_read_count) &&
        sector + block_count <= cur.cache_start_sector + cur.cache_block_count)
      use = self->current;
    else
      for (i = 0; i < READ_CACHE_CHUNKS; i++)
        if (self->chunk[i].cache_valid &&
            sector >= self->chunk[i].cache_start_sector &&
            sector <= (self->chunk[i].cache_start_sector + self->chunk[i].cache_read_count) &&
            sector + block_count <= self->chunk[i].cache_start_sector + self->chunk[i].cache_block_count)
            use = i;
  }

  if (use >= 0) {
    read_cache_chunk_t *chunk;
    
    /* Increment read-ahead size if sector follows the last sector */
    if (sector == (self->last_sector + 1)) {
      self->read_ahead_incr++;
    } else {
      self->read_ahead_size = READ_AHEAD_SIZE_MIN;
      self->read_ahead_incr = 0;
    }
    self->last_sector = sector;

    /* The following resources need to be protected by a mutex :
     *   self->chunk[*].cache_buffer
     *   self->chunk[*].cache_malloc_size
     *   self->chunk[*].usage_count
     */
    pthread_mutex_lock(&self->lock);
    chunk = &self->chunk[use];
    read_ahead_buf = chunk->cache_buffer + chunk->cache_read_count * DVD_VIDEO_LB_LEN;
    *buf = chunk->cache_buffer + (sector - chunk->cache_start_sector) * DVD_VIDEO_LB_LEN;
    chunk->usage_count++;
    pthread_mutex_unlock(&self->lock);

    /* Read blocks if needed */
    if (sector >= (chunk->cache_start_sector + chunk->cache_read_count)) {

      dprintf("libdvdnav: sector=%d, start_sector=%d, last_sector=%d\n", sector, chunk->cache_start_sector, chunk->cache_start_sector + chunk->cache_block_count);

      /* read_ahead_size */
      incr = self->read_ahead_incr >> 1;
      if ((self->read_ahead_size + incr) > READ_AHEAD_SIZE_MAX) {
        self->read_ahead_size = READ_AHEAD_SIZE_MAX;
      } else {
        self->read_ahead_size = self->read_ahead_size + incr;
      }
      self->read_ahead_incr = 0;

      /* real read size */
      start = chunk->cache_start_sector + chunk->cache_read_count;
      if (chunk->cache_read_count + self->read_ahead_size > chunk->cache_block_count) {
        size = chunk->cache_block_count - chunk->cache_read_count;
      } else {
        size = self->read_ahead_size;
      }
      dprintf("libdvdnav: read_ahead_size=%d, size=%d\n", self->read_ahead_size, size);

      chunk->cache_read_count += DVDReadBlocks(self->dvd_self->file,
                                               start,
                                               size,
                                               read_ahead_buf);
    }

    res = DVD_VIDEO_LB_LEN * block_count;

  } else {

    if (self->dvd_self->use_read_ahead)
      dprintf("cache miss on sector %d\n", sector);

    res = DVDReadBlocks(self->dvd_self->file,
                        sector,
                        block_count,
                        *buf) * DVD_VIDEO_LB_LEN;
  }

  return res;

}

dvdnav_status_t dvdnav_free_cache_block(dvdnav_t *self, unsigned char *buf) {
  read_cache_t *cache;
  int i;

  if (!self)
    return DVDNAV_STATUS_ERR;

  cache = self->cache;
  if (!cache)
    return DVDNAV_STATUS_ERR;

  pthread_mutex_lock(&cache->lock);
  for (i = 0; i < READ_CACHE_CHUNKS; i++) {
    if (cache->chunk[i].cache_buffer && buf >= cache->chunk[i].cache_buffer &&
        buf < cache->chunk[i].cache_buffer + cache->chunk[i].cache_malloc_size * DVD_VIDEO_LB_LEN) {
      cache->chunk[i].usage_count--;
    }
  }
  pthread_mutex_unlock(&cache->lock);

  if (cache->freeing)
    /* when we want to dispose the cache, try freeing it now */
    dvdnav_read_cache_free(cache);

  return DVDNAV_STATUS_OK;
}

#endif
