/*
 * Copyright (C) 2000, 2001 Håkan Hjort
 * Copyright (C) 2001 Rich Wareham <richwareham@users.sourceforge.net>
 * 
 * This file is part of libdvdnav, a DVD navigation library. It is modified
 * from a file originally part of the Ogle DVD player.
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

#ifndef VM_H_INCLUDED
#define VM_H_INCLUDED

#include "decoder.h"
#include <dvd_types.h>

/* DOMAIN enum */

typedef enum {
  FP_DOMAIN = 1,
  VTS_DOMAIN = 2,
  VMGM_DOMAIN = 4,
  VTSM_DOMAIN = 8
} domain_t;  

/**
 * State: SPRM, GPRM, Domain, pgc, pgN, cellN, ?
 */
typedef struct {
  registers_t registers;
  
  pgc_t *pgc; /*  either this or *pgc is enough? */
  
  domain_t domain;
  int vtsN; /*  0 is vmgm? */
  /*   int pgcN; // either this or *pgc is enough. Which to use? */
  int pgN;  /*  is this needed? can allways fid pgN from cellN? */
  int cellN;
  int blockN;
  
  /* Resume info */
  int rsm_vtsN;
  int rsm_blockN; /* of nav_packet */
  uint16_t rsm_regs[5]; /* system registers 4-8 */
  int rsm_pgcN;
  int rsm_cellN;
} dvd_state_t;

typedef struct {
  dvd_reader_t *dvd;
  ifo_handle_t *vmgi;
  ifo_handle_t *vtsi;
  dvd_state_t   state;
  int  badness_counter;
} vm_t;


/*  Audio stream number */
#define AST_REG      registers.SPRM[1]
/*  Subpicture stream number */
#define SPST_REG     registers.SPRM[2]
/*  Angle number */
#define AGL_REG      registers.SPRM[3]
/*  Title Track Number */
#define TTN_REG      registers.SPRM[4]
/*  VTS Title Track Number */
#define VTS_TTN_REG  registers.SPRM[5]
/*  PGC Number for this Title Track */
#define TT_PGCN_REG  registers.SPRM[6]
/*  Current Part of Title (PTT) number for (One_Sequential_PGC_Title) */
#define PTTN_REG     registers.SPRM[7]
/*  Highlighted Button Number (btn nr 1 == value 1024) */
#define HL_BTNN_REG  registers.SPRM[8]
/*  Parental Level */
#define PTL_REG      registers.SPRM[13]

/* Initialisation & destruction */
vm_t* vm_new_vm();
void vm_free_vm(vm_t *self);

/* IFO access */
ifo_handle_t *vm_get_vmgi(vm_t *self);
ifo_handle_t *vm_get_vtsi(vm_t *self);

/* Reader Access */
dvd_reader_t *vm_get_dvd_reader(vm_t *self);

/* Jumping */
int vm_start_title(vm_t *self, int tt);
int vm_jump_prog(vm_t *self, int pr);

/* Other calls */
int vm_reset(vm_t *self, char *dvdroot); /*  , register_t regs); */
int vm_start(vm_t *self);
int vm_eval_cmd(vm_t *self, vm_cmd_t *cmd);
int vm_get_next_cell(vm_t *self);
int vm_menu_call(vm_t *self, DVDMenuID_t menuid, int block);
int vm_resume(vm_t *self);
int vm_go_up(vm_t *self);
int vm_top_pg(vm_t *self);
int vm_next_pg(vm_t *self);
int vm_prev_pg(vm_t *self);
int vm_get_audio_stream(vm_t *self, int audioN);
int vm_get_audio_active_stream(vm_t *self);
int vm_get_subp_stream(vm_t *self, int subpN);
int vm_get_subp_active_stream(vm_t *self);
void vm_get_angle_info(vm_t *self, int *num_avail, int *current);
void vm_get_audio_info(vm_t *self, int *num_avail, int *current);
void vm_get_subp_info(vm_t *self, int *num_avail, int *current);
subp_attr_t vm_get_subp_attr(vm_t *self, int streamN);
audio_attr_t vm_get_audio_attr(vm_t *self, int streamN);
void vm_get_video_res(vm_t *self, int *width, int *height);

#endif /* VM_HV_INCLUDED */

