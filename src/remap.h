#ifndef __REMAP__H
#define __REMAP__H
typedef struct block_s block_t;

typedef struct remap_s remap_t;

remap_t* remap_loadmap( char *title);

unsigned long remap_block( 
	remap_t *map, int domain, int title, int program, 
	unsigned long cblock, unsigned long offset);

#endif
