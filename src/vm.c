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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>

#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>

#include "decoder.h"
#include "vmcmd.h"
#include "vm.h"

/* Local prototypes */

static void saveRSMinfo(vm_t *self,int cellN, int blockN);
static int set_PGN(vm_t *self);
static link_t play_PGC(vm_t *self);
static link_t play_PG(vm_t *self);
static link_t play_Cell(vm_t *self);
static link_t play_Cell_post(vm_t *self);
static link_t play_PGC_post(vm_t *self);
static link_t process_command(vm_t *self,link_t link_values);

static void ifoOpenNewVTSI(vm_t *self,dvd_reader_t *dvd, int vtsN);
static pgcit_t* get_PGCIT(vm_t *self);
static int get_video_aspect(vm_t *self);

/* Can only be called when in VTS_DOMAIN */
static int get_TT(vm_t *self,int tt);
static int get_VTS_TT(vm_t *self,int vtsN, int vts_ttn);
static int get_VTS_PTT(vm_t *self,int vtsN, int vts_ttn, int part);

static int get_MENU(vm_t *self,int menu); /*  VTSM & VMGM */
static int get_FP_PGC(vm_t *self); /*  FP */

/* Called in any domain */
static int get_ID(vm_t *self,int id);
static int get_PGC(vm_t *self,int pgcN);
static int get_PGCN(vm_t *self);

/* Initialisation */

vm_t* vm_new_vm() {
  vm_t *vm = (vm_t*)calloc(sizeof(vm_t), sizeof(char));

  return vm;
}

static void vm_print_current_domain_state(vm_t *self) {
  switch((self->state).domain) {
    case VTS_DOMAIN:
      fprintf(stderr, "Video Title Domain: -\n");
      break;

    case VTSM_DOMAIN:
      fprintf(stderr, "Video Title Menu Domain: -\n");
      break;

    case VMGM_DOMAIN:
      fprintf(stderr, "Video Manager Menu Domain: -\n");
      break;

    case FP_DOMAIN: 
      fprintf(stderr, "First Play Domain: -\n");
      break;

    default:
      fprintf(stderr, "Unknown Domain: -\n");
      break;
  }
  fprintf(stderr, "VTS:%d PG:%u CELL:%u BLOCK:%u VTS_TTN:%u TTN:%u TT_PGCN:%u\n", 
                   (self->state).vtsN,
                   (self->state).pgN,
                   (self->state).cellN,
                   (self->state).blockN,
                   (self->state).VTS_TTN_REG,
                   (self->state).TTN_REG,
                   (self->state).TT_PGCN_REG);
}

void vm_stop(vm_t *self) {
  if(!self)
   return;

  if(self->vmgi) {
    ifoClose(self->vmgi);
    self->vmgi=NULL;
  }

  if(self->vtsi) {
    ifoClose(self->vtsi);
    self->vmgi=NULL;
  }
  
  if(self->dvd) {
    DVDClose(self->dvd);
    self->dvd=NULL;
  }
}

void vm_free_vm(vm_t *self) {
  if(self) {
    vm_stop(self);
    free(self);
  }
}

/* IFO Access */

ifo_handle_t *vm_get_vmgi(vm_t *self) {
  if(!self)
   return NULL;
  
  return self->vmgi;
}

ifo_handle_t *vm_get_vtsi(vm_t *self) {
  if(!self)
   return NULL;
  
  return self->vtsi;
}

/* Reader Access */

dvd_reader_t *vm_get_dvd_reader(vm_t *self) {
  if(!self)
   return NULL;
  
  return self->dvd;
}

int vm_reset(vm_t *self, char *dvdroot) /*  , register_t regs) */ { 
  /*  Setup State */
  memset((self->state).registers.SPRM, 0, sizeof(uint16_t)*24);
  memset((self->state).registers.GPRM, 0, sizeof((self->state).registers.GPRM));
  (self->state).registers.SPRM[0] = ('e'<<8)|'n'; /*  Player Menu Languange code */
  (self->state).AST_REG = 15; /*  15 why? */
  (self->state).SPST_REG = 62; /*  62 why? */
  (self->state).AGL_REG = 1;
  (self->state).TTN_REG = 1;
  (self->state).VTS_TTN_REG = 1;
  /* (self->state).TT_PGCN_REG = 0 */
  (self->state).PTTN_REG = 1;
  (self->state).HL_BTNN_REG = 1 << 10;

  (self->state).PTL_REG = 15; /*  Parental Level */
  (self->state).registers.SPRM[12] = ('U'<<8)|'S'; /*  Parental Management Country Code */
  (self->state).registers.SPRM[16] = ('e'<<8)|'n'; /*  Initial Language Code for Audio */
  (self->state).registers.SPRM[18] = ('e'<<8)|'n'; /*  Initial Language Code for Spu */
  /*  Player Regional Code Mask. 
   *  bit0 = Region 1
   *  bit1 = Region 2
   */
  (self->state).registers.SPRM[20] = 0x1; /*  Player Regional Code Mask. Region free! */
  (self->state).registers.SPRM[14] = 0x100; /* Try Pan&Scan */
   
  (self->state).pgN = 0;
  (self->state).cellN = 0;

  (self->state).domain = FP_DOMAIN;
  (self->state).rsm_vtsN = 0;
  (self->state).rsm_cellN = 0;
  (self->state).rsm_blockN = 0;
  
  (self->state).vtsN = -1;
  
  if (self->dvd && dvdroot) {
    // a new dvd device has been requested
    vm_stop(self);
  }
  if (!self->dvd) {
    self->dvd = DVDOpen(dvdroot);
    if(!self->dvd) {
      fprintf(stderr, "vm: faild to open/read the DVD\n");
      return -1;
    }

    self->vmgi = ifoOpenVMGI(self->dvd);
    if(!self->vmgi) {
      fprintf(stderr, "vm: faild to read VIDEO_TS.IFO\n");
      return -1;
    }
    if(!ifoRead_FP_PGC(self->vmgi)) {
      fprintf(stderr, "vm: ifoRead_FP_PGC failed\n");
      return -1;
    }
    if(!ifoRead_TT_SRPT(self->vmgi)) {
      fprintf(stderr, "vm: ifoRead_TT_SRPT failed\n");
      return -1;
    }
    if(!ifoRead_PGCI_UT(self->vmgi)) {
      fprintf(stderr, "vm: ifoRead_PGCI_UT failed\n");
      return -1;
    }
    if(!ifoRead_PTL_MAIT(self->vmgi)) {
      fprintf(stderr, "vm: ifoRead_PTL_MAIT failed\n");
      ; /*  return -1; Not really used for now.. */
    }
    if(!ifoRead_VTS_ATRT(self->vmgi)) {
      fprintf(stderr, "vm: ifoRead_VTS_ATRT failed\n");
      ; /*  return -1; Not really used for now.. */
    }
    if(!ifoRead_VOBU_ADMAP(self->vmgi)) {
      fprintf(stderr, "vm: ifoRead_VOBU_ADMAP vgmi failed\n");
      ; /*  return -1; Not really used for now.. */
    }
    /* ifoRead_TXTDT_MGI(vmgi); Not implemented yet */
  }
  else fprintf(stderr, "vm: reset\n");

  return 0;
}

/*  FIXME TODO XXX $$$ Handle error condition too... */
int vm_start(vm_t *self)
{
  link_t link_values;

  /*  Set pgc to FP pgc */
  get_FP_PGC(self);
  link_values = play_PGC(self); 
  link_values = process_command(self,link_values);
  assert(link_values.command == PlayThis);
  (self->state).blockN = link_values.data1;

  return 0; /* ?? */
}

int vm_start_title(vm_t *self, int tt) {
  link_t link_values;

  get_TT(self, tt);
  link_values = play_PGC(self); 
  link_values = process_command(self, link_values);
  assert(link_values.command == PlayThis);
  (self->state).blockN = link_values.data1;

  return 0; /* ?? */
}

int vm_jump_prog(vm_t *self, int pr) {
  link_t link_values;

  (self->state).pgN = pr; /*  ?? */

  get_PGC(self, get_PGCN(self));
  link_values = play_PG(self); 
  link_values = process_command(self, link_values);
  assert(link_values.command == PlayThis);
  (self->state).blockN = link_values.data1;
  
  return 0; /* ?? */
}

int vm_eval_cmd(vm_t *self, vm_cmd_t *cmd)
{
  link_t link_values;
  
  if(vmEval_CMD(cmd, 1, &(self->state).registers, &link_values)) {
    link_values = process_command(self, link_values);
    assert(link_values.command == PlayThis);
    (self->state).blockN = link_values.data1;
    return 1; /*  Something changed, Jump */
  } else {
    return 0; /*  It updated some state thats all... */
  }
}

int vm_get_next_cell(vm_t *self)
{
  link_t link_values;
  link_values = play_Cell_post(self);
  link_values = process_command(self,link_values);
  assert(link_values.command == PlayThis);
  (self->state).blockN = link_values.data1;
  
  return 0; /*  ?? */
}

int vm_top_pg(vm_t *self)
{
  link_t link_values;
  link_values = play_PG(self);
  link_values = process_command(self,link_values);
  assert(link_values.command == PlayThis);
  (self->state).blockN = link_values.data1;
  
  return 1; /*  Jump */
}

int vm_go_up(vm_t *self)
{
  link_t link_values;
 
  if(get_PGC(self, (self->state).pgc->goup_pgc_nr))
   assert(0);

  link_values = play_PGC(self);
  link_values = process_command(self,link_values);
  assert(link_values.command == PlayThis);
  (self->state).blockN = link_values.data1;
  
  return 1; /*  Jump */
}

int vm_next_pg(vm_t *self)
{
  /*  Do we need to get a updated pgN value first? */
  (self->state).pgN += 1; 
  return vm_top_pg(self);
}

int vm_prev_pg(vm_t *self)
{
  /*  Do we need to get a updated pgN value first? */
  (self->state).pgN -= 1;
  if((self->state).pgN == 0) {
    /*  Check for previous PGCN ?  */
    (self->state).pgN = 1;
    /*  return 0; */
  }
  return vm_top_pg(self);
}


static domain_t menuid2domain(DVDMenuID_t menuid)
{
  domain_t result = VTSM_DOMAIN; /*  Really shouldn't have to.. */
  
  switch(menuid) {
  case DVD_MENU_Title:
    result = VMGM_DOMAIN;
    break;
  case DVD_MENU_Root:
  case DVD_MENU_Subpicture:
  case DVD_MENU_Audio:
  case DVD_MENU_Angle:
  case DVD_MENU_Part:
    result = VTSM_DOMAIN;
    break;
  }
  
  return result;
}

int vm_menu_call(vm_t *self, DVDMenuID_t menuid, int block)
{
  domain_t old_domain;
  link_t link_values;
  
  /* Should check if we are allowed/can acces this menu */
  
  
  /* FIXME XXX $$$ How much state needs to be restored 
   * when we fail to find a menu? */
  
  old_domain = (self->state).domain;
  
  switch((self->state).domain) {
  case VTS_DOMAIN:
    saveRSMinfo(self, 0, block);
    /* FALL THROUGH */
  case VTSM_DOMAIN:
  case VMGM_DOMAIN:
    (self->state).domain = menuid2domain(menuid);
    if(get_PGCIT(self) != NULL && get_MENU(self, menuid) != -1) {
      link_values = play_PGC(self);
      link_values = process_command(self, link_values);
      assert(link_values.command == PlayThis);
      (self->state).blockN = link_values.data1;
      return 1; /*  Jump */
    } else {
      (self->state).domain = old_domain;
    }
    break;
  case FP_DOMAIN: /* FIXME XXX $$$ What should we do here? */
    break;
  }
  
  return 0;
}


int vm_resume(vm_t *self)
{
  int i;
  link_t link_values;
  
  /*  Check and see if there is any rsm info!! */
  if((self->state).rsm_vtsN == 0) {
    return 0;
  }
  
  (self->state).domain = VTS_DOMAIN;
  ifoOpenNewVTSI(self, self->dvd, (self->state).rsm_vtsN);
  get_PGC(self, (self->state).rsm_pgcN);
  
  /* These should never be set in SystemSpace and/or MenuSpace */ 
  /*  (self->state).TTN_REG = (self->state).rsm_tt; */
  /*  (self->state).TT_PGCN_REG = (self->state).rsm_pgcN; */
  /*  (self->state).HL_BTNN_REG = (self->state).rsm_btnn; */
  for(i = 0; i < 5; i++) {
    (self->state).registers.SPRM[4 + i] = (self->state).rsm_regs[i];
  }

  if((self->state).rsm_cellN == 0) {
    assert((self->state).cellN); /*  Checking if this ever happens */
    (self->state).pgN = 1;
    link_values = play_PG(self);
    link_values = process_command(self, link_values);
    assert(link_values.command == PlayThis);
    (self->state).blockN = link_values.data1;
  } else { 
    (self->state).cellN = (self->state).rsm_cellN;
    (self->state).blockN = (self->state).rsm_blockN;
    /* (self->state).pgN = ?? does this gets the righ value in play_Cell, no! */
    if(set_PGN(self)) {
      /* Were at or past the end of the PGC, should not happen for a RSM */
      assert(0);
      play_PGC_post(self);
    }
  }
  
  return 1; /*  Jump */
}

/**
 * Return the substream id for 'logical' audio stream audioN.
 *  0 <= audioN < 8
 */
int vm_get_audio_stream(vm_t *self, int audioN)
{
  int streamN = -1;
  fprintf(stderr,"dvdnav:vm.c:get_audio_stream audioN=%d\n",audioN); 
  if((self->state).domain == VTSM_DOMAIN 
     || (self->state).domain == VMGM_DOMAIN
     || (self->state).domain == FP_DOMAIN) {
    audioN = 0;
  }
  
  if(audioN < 8) {
    /* Is there any contol info for this logical stream */ 
    if((self->state).pgc->audio_control[audioN] & (1<<15)) {
      streamN = ((self->state).pgc->audio_control[audioN] >> 8) & 0x07;  
    }
  }
  
  if((self->state).domain == VTSM_DOMAIN 
     || (self->state).domain == VMGM_DOMAIN
     || (self->state).domain == FP_DOMAIN) {
    if(streamN == -1)
      streamN = 0;
  }
  
  /* Should also check in vtsi/vmgi status that what kind of stream
   * it is (ac3/lpcm/dts/sdds...) to find the right (sub)stream id */
  return streamN;
}

/**
 * Return the substream id for 'logical' subpicture stream subpN.
 * 0 <= subpN < 32
 */
int vm_get_subp_stream(vm_t *self, int subpN)
{
  int streamN = -1;
  int source_aspect = get_video_aspect(self);
  
  if((self->state).domain == VTSM_DOMAIN 
     || (self->state).domain == VMGM_DOMAIN
     || (self->state).domain == FP_DOMAIN) {
    subpN = 0;
  }
  
  if(subpN < 32) { /* a valid logical stream */
    /* Is this logical stream present */ 
    if((self->state).pgc->subp_control[subpN] & (1<<31)) {
      if(source_aspect == 0) /* 4:3 */	     
	streamN = ((self->state).pgc->subp_control[subpN] >> 24) & 0x1f;  
      if(source_aspect == 3) /* 16:9 */
	streamN = ((self->state).pgc->subp_control[subpN] >> 16) & 0x1f;
    }
  }
  
  /* Paranoia.. if no stream select 0 anyway */
/* I am not paranoid */
/* if((self->state).domain == VTSM_DOMAIN 
     || (self->state).domain == VMGM_DOMAIN
     || (self->state).domain == FP_DOMAIN) {
    if(streamN == -1)
      streamN = 0;
  }
*/
  /* Should also check in vtsi/vmgi status that what kind of stream it is. */
  return streamN;
}

int vm_get_subp_active_stream(vm_t *self)
{
  int subpN;
  int streamN;
  subpN = (self->state).SPST_REG & ~0x40;
  streamN = vm_get_subp_stream(self, subpN);
  
  /* If no such stream, then select the first one that exists. */
  if(streamN == -1) {
    for(subpN = 0; subpN < 32; subpN++) {
      if((self->state).pgc->subp_control[subpN] & (1<<31)) {
      
        streamN = vm_get_subp_stream(self, subpN);
        break;
      }
    }
  } 
  
  /* We should instead send the on/off status to the spudecoder / mixer */
  /* If we are in the title domain see if the spu mixing is on */
  if((self->state).domain == VTS_DOMAIN && !((self->state).SPST_REG & 0x40)) { 
     /* Bit 7 set means hide, and only let Forced display show */
     return (streamN | 0x80); 
  } else {
    return streamN;
  }
}

int vm_get_audio_active_stream(vm_t *self)
{
  int audioN;
  int streamN;
  audioN = (self->state).AST_REG ;
  streamN = vm_get_audio_stream(self, audioN);
  
  /* If no such stream, then select the first one that exists. */
  if(streamN == -1) {
    for(audioN = 0; audioN < 8; audioN++) {
      if((self->state).pgc->audio_control[audioN] & (1<<15)) {
        streamN = vm_get_audio_stream(self, audioN);
        break;
      }
    }
  } 
  
  return streamN;
}


void vm_get_angle_info(vm_t *self, int *num_avail, int *current)
{
  *num_avail = 1;
  *current = 1;
  
  if((self->state).domain == VTS_DOMAIN) {
    /*  TTN_REG does not allways point to the correct title.. */
    title_info_t *title;
    if((self->state).TTN_REG > self->vmgi->tt_srpt->nr_of_srpts)
      return;
    title = &self->vmgi->tt_srpt->title[(self->state).TTN_REG - 1];
    if(title->title_set_nr != (self->state).vtsN || 
       title->vts_ttn != (self->state).VTS_TTN_REG)
      return; 
    *num_avail = title->nr_of_angles;
    *current = (self->state).AGL_REG;
    if(*current > *num_avail) /*  Is this really a good idea? */
      *current = *num_avail; 
  }
}


void vm_get_audio_info(vm_t *self, int *num_avail, int *current)
{
  if((self->state).domain == VTS_DOMAIN) {
    *num_avail = self->vtsi->vtsi_mat->nr_of_vts_audio_streams;
    *current = (self->state).AST_REG;
  } else if((self->state).domain == VTSM_DOMAIN) {
    *num_avail = self->vtsi->vtsi_mat->nr_of_vtsm_audio_streams; /*  1 */
    *current = 1;
  } else if((self->state).domain == VMGM_DOMAIN || (self->state).domain == FP_DOMAIN) {
    *num_avail = self->vmgi->vmgi_mat->nr_of_vmgm_audio_streams; /*  1 */
    *current = 1;
  }
}

void vm_get_subp_info(vm_t *self, int *num_avail, int *current)
{
  if((self->state).domain == VTS_DOMAIN) {
    *num_avail = self->vtsi->vtsi_mat->nr_of_vts_subp_streams;
    *current = (self->state).SPST_REG;
  } else if((self->state).domain == VTSM_DOMAIN) {
    *num_avail = self->vtsi->vtsi_mat->nr_of_vtsm_subp_streams; /*  1 */
    *current = 0x41;
  } else if((self->state).domain == VMGM_DOMAIN || (self->state).domain == FP_DOMAIN) {
    *num_avail = self->vmgi->vmgi_mat->nr_of_vmgm_subp_streams; /*  1 */
    *current = 0x41;
  }
}

subp_attr_t vm_get_subp_attr(vm_t *self, int streamN)
{
  subp_attr_t attr;
  
  if((self->state).domain == VTS_DOMAIN) {
    attr = self->vtsi->vtsi_mat->vts_subp_attr[streamN];
  } else if((self->state).domain == VTSM_DOMAIN) {
    attr = self->vtsi->vtsi_mat->vtsm_subp_attr;
  } else if((self->state).domain == VMGM_DOMAIN || (self->state).domain == FP_DOMAIN) {
    attr = self->vmgi->vmgi_mat->vmgm_subp_attr;
  }
  return attr;
}

audio_attr_t vm_get_audio_attr(vm_t *self, int streamN)
{
  audio_attr_t attr;
  
  if((self->state).domain == VTS_DOMAIN) {
    attr = self->vtsi->vtsi_mat->vts_audio_attr[streamN];
  } else if((self->state).domain == VTSM_DOMAIN) {
    attr = self->vtsi->vtsi_mat->vtsm_audio_attr;
  } else if((self->state).domain == VMGM_DOMAIN || (self->state).domain == FP_DOMAIN) {
    attr = self->vmgi->vmgi_mat->vmgm_audio_attr;
  }
  return attr;
}

video_attr_t vm_get_video_attr(vm_t *self)
{
  video_attr_t attr;
  
  if((self->state).domain == VTS_DOMAIN) {
    attr = self->vtsi->vtsi_mat->vts_video_attr;
  } else if((self->state).domain == VTSM_DOMAIN) {
    attr = self->vtsi->vtsi_mat->vtsm_video_attr;
  } else if((self->state).domain == VMGM_DOMAIN || (self->state).domain == FP_DOMAIN) {
    attr = self->vmgi->vmgi_mat->vmgm_video_attr;
  }
  return attr;
}

void vm_get_video_res(vm_t *self, int *width, int *height)
{
  video_attr_t attr;
  
  attr = vm_get_video_attr(self);
  
  if(attr.video_format != 0) 
    *height = 576;
  else
    *height = 480;
  switch(attr.picture_size) {
  case 0:
    *width = 720;
    break;
  case 1:
    *width = 704;
    break;
  case 2:
    *width = 352;
    break;
  case 3:
    *width = 352;
    *height /= 2;
    break;
  }
}

/*  Must be called before domain is changed (get_PGCN()) */
static void saveRSMinfo(vm_t *self, int cellN, int blockN)
{
  int i;
  
  if(cellN != 0) {
    (self->state).rsm_cellN = cellN;
    (self->state).rsm_blockN = 0;
  } else {
    (self->state).rsm_cellN = (self->state).cellN;
    (self->state).rsm_blockN = blockN;
  }
  (self->state).rsm_vtsN = (self->state).vtsN;
  (self->state).rsm_pgcN = get_PGCN(self);
  
  /* assert((self->state).rsm_pgcN == (self->state).TT_PGCN_REG); // for VTS_DOMAIN */
  
  for(i = 0; i < 5; i++) {
    (self->state).rsm_regs[i] = (self->state).registers.SPRM[4 + i];
  }
}



/* Figure out the correct pgN from the cell and update (self->state). */ 
static int set_PGN(vm_t *self) {
  int new_pgN = 0;
  
  while(new_pgN < (self->state).pgc->nr_of_programs 
	&& (self->state).cellN >= (self->state).pgc->program_map[new_pgN])
    new_pgN++;
  
  if(new_pgN == (self->state).pgc->nr_of_programs) /* We are at the last program */
    if((self->state).cellN > (self->state).pgc->nr_of_cells)
      return 1; /* We are past the last cell */
  
  (self->state).pgN = new_pgN;
  
  if((self->state).domain == VTS_DOMAIN) {
    playback_type_t *pb_ty;
    if((self->state).TTN_REG > self->vmgi->tt_srpt->nr_of_srpts)
      return 0; /*  ?? */
    pb_ty = &self->vmgi->tt_srpt->title[(self->state).TTN_REG - 1].pb_ty;
    if(pb_ty->multi_or_random_pgc_title == /* One_Sequential_PGC_Title */ 0) {
#if 0 /* TTN_REG can't be trusted to have a correct value here... */
      vts_ptt_srpt_t *ptt_srpt = vtsi->vts_ptt_srpt;
      assert((self->state).VTS_TTN_REG <= ptt_srpt->nr_of_srpts);
      assert(get_PGCN() == ptt_srpt->title[(self->state).VTS_TTN_REG - 1].ptt[0].pgcn);
      assert(1 == ptt_srpt->title[(self->state).VTS_TTN_REG - 1].ptt[0].pgn);
#endif
      (self->state).PTTN_REG = (self->state).pgN;
    }
  }
  
  return 0;
}





static link_t play_PGC(vm_t *self) 
{    
  link_t link_values;
  
  fprintf(stderr, "vm: play_PGC:");
  if((self->state).domain != FP_DOMAIN)
    fprintf(stderr, " (self->state).pgcN (%i)\n", get_PGCN(self));
  else
    fprintf(stderr, " first_play_pgc\n");

  /*  This must be set before the pre-commands are executed because they */
  /*  might contain a CallSS that will save resume state */
  (self->state).pgN = 1;
  (self->state).cellN = 0;

  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - just play video i.e first PG
       (This is what happens if you fall of the end of the pre_cmds)
     - or a error (are there more cases?) */
  if((self->state).pgc->command_tbl && (self->state).pgc->command_tbl->nr_of_pre) {
    if(vmEval_CMD((self->state).pgc->command_tbl->pre_cmds, 
		  (self->state).pgc->command_tbl->nr_of_pre, 
		  &(self->state).registers, &link_values)) {
      /*  link_values contains the 'jump' return value */
      return link_values;
    } else {
      fprintf(stderr, "PGC pre commands didn't do a Jump, Link or Call\n");
    }
  }
  return play_PG(self);
}  


static link_t play_PG(vm_t *self)
{
  fprintf(stderr, "play_PG: (self->state).pgN (%i)\n", (self->state).pgN);
  
  assert((self->state).pgN > 0);
  if((self->state).pgN > (self->state).pgc->nr_of_programs) {
    fprintf(stderr, "(self->state).pgN (%i) == pgc->nr_of_programs + 1 (%i)\n", 
	    (self->state).pgN, (self->state).pgc->nr_of_programs + 1);
    assert((self->state).pgN == (self->state).pgc->nr_of_programs + 1);
    return play_PGC_post(self);
  }
  
  (self->state).cellN = (self->state).pgc->program_map[(self->state).pgN - 1];
  
  return play_Cell(self);
}


static link_t play_Cell(vm_t *self)
{
  fprintf(stderr, "play_Cell: (self->state).cellN (%i)\n", (self->state).cellN);
  
  assert((self->state).cellN > 0);
  if((self->state).cellN > (self->state).pgc->nr_of_cells) {
    fprintf(stderr, "(self->state).cellN (%i) == pgc->nr_of_cells + 1 (%i)\n", 
	    (self->state).cellN, (self->state).pgc->nr_of_cells + 1);
    assert((self->state).cellN == (self->state).pgc->nr_of_cells + 1); 
    return play_PGC_post(self);
  }
  

  /* Multi angle/Interleaved */
  switch((self->state).pgc->cell_playback[(self->state).cellN - 1].block_mode) {
  case 0: /*  Normal */
    assert((self->state).pgc->cell_playback[(self->state).cellN - 1].block_type == 0);
    break;
  case 1: /*  The first cell in the block */
    switch((self->state).pgc->cell_playback[(self->state).cellN - 1].block_type) {
    case 0: /*  Not part of a block */
      assert(0);
    case 1: /*  Angle block */
      /* Loop and check each cell instead? So we don't get outsid the block. */
      (self->state).cellN += (self->state).AGL_REG - 1;
      assert((self->state).cellN <= (self->state).pgc->nr_of_cells);
      assert((self->state).pgc->cell_playback[(self->state).cellN - 1].block_mode != 0);
      assert((self->state).pgc->cell_playback[(self->state).cellN - 1].block_type == 1);
      break;
    case 2: /*  ?? */
    case 3: /*  ?? */
    default:
      fprintf(stderr, "Invalid? Cell block_mode (%d), block_type (%d)\n",
	      (self->state).pgc->cell_playback[(self->state).cellN - 1].block_mode,
	      (self->state).pgc->cell_playback[(self->state).cellN - 1].block_type);
    }
    break;
  case 2: /*  Cell in the block */
  case 3: /*  Last cell in the block */
  /*  These might perhaps happen for RSM or LinkC commands? */
  default:
    fprintf(stderr, "Cell is in block but did not enter at first cell!\n");
  }
  
  /* Updates (self->state).pgN and PTTN_REG */
  if(set_PGN(self)) {
    /* Should not happen */
    link_t tmp = {LinkTailPGC, /* No Button */ 0, 0, 0};
    assert(0);
    return tmp;
  }
  
  {
    link_t tmp = {PlayThis, /* Block in Cell */ 0, 0, 0};
    return tmp;
  }

}

static link_t play_Cell_post(vm_t *self)
{
  cell_playback_t *cell;
  
  fprintf(stderr, "play_Cell_post: (self->state).cellN (%i)\n", (self->state).cellN);
  
  cell = &(self->state).pgc->cell_playback[(self->state).cellN - 1];
  
  /* Still time is already taken care of before we get called. */
  
  /* Deal with a Cell command, if any */
  if(cell->cell_cmd_nr != 0) {
    link_t link_values;
    
    assert((self->state).pgc->command_tbl != NULL);
    assert((self->state).pgc->command_tbl->nr_of_cell >= cell->cell_cmd_nr);
    fprintf(stderr, "Cell command pressent, executing\n");
    if(vmEval_CMD(&(self->state).pgc->command_tbl->cell_cmds[cell->cell_cmd_nr - 1], 1,
		  &(self->state).registers, &link_values)) {
      return link_values;
    } else {
       fprintf(stderr, "Cell command didn't do a Jump, Link or Call\n");
      /*  Error ?? goto tail? goto next PG? or what? just continue? */
    }
  }
  
  
  /* Where to continue after playing the cell... */
  /* Multi angle/Interleaved */
  switch((self->state).pgc->cell_playback[(self->state).cellN - 1].block_mode) {
  case 0: /*  Normal */
    assert((self->state).pgc->cell_playback[(self->state).cellN - 1].block_type == 0);
    (self->state).cellN++;
    break;
  case 1: /*  The first cell in the block */
  case 2: /*  A cell in the block */
  case 3: /*  The last cell in the block */
  default:
    switch((self->state).pgc->cell_playback[(self->state).cellN - 1].block_type) {
    case 0: /*  Not part of a block */
      assert(0);
    case 1: /*  Angle block */
      /* Skip the 'other' angles */
      (self->state).cellN++;
      while((self->state).cellN <= (self->state).pgc->nr_of_cells 
	    && (self->state).pgc->cell_playback[(self->state).cellN - 1].block_mode >= 2) {
	(self->state).cellN++;
      }
      break;
    case 2: /*  ?? */
    case 3: /*  ?? */
    default:
      fprintf(stderr, "Invalid? Cell block_mode (%d), block_type (%d)\n",
	      (self->state).pgc->cell_playback[(self->state).cellN - 1].block_mode,
	      (self->state).pgc->cell_playback[(self->state).cellN - 1].block_type);
    }
    break;
  }
  
  
  /* Figure out the correct pgN for the new cell */ 
  if(set_PGN(self)) {
    fprintf(stderr, "last cell in this PGC\n");
    return play_PGC_post(self);
  }

  return play_Cell(self);
}


static link_t play_PGC_post(vm_t *self)
{
  link_t link_values;

  fprintf(stderr, "play_PGC_post:\n");
  
  assert((self->state).pgc->still_time == 0); /*  FIXME $$$ */

  /* eval -> updates the state and returns either 
     - some kind of jump (Jump(TT/SS/VTS_TTN/CallSS/link C/PG/PGC/PTTN)
     - or a error (are there more cases?)
     - if you got to the end of the post_cmds, then what ?? */
  if((self->state).pgc->command_tbl &&
     vmEval_CMD((self->state).pgc->command_tbl->post_cmds,
		(self->state).pgc->command_tbl->nr_of_post, 
		&(self->state).registers, &link_values)) {
    return link_values;
  }
  
  /*  Or perhaps handle it here? */
  {
    link_t link_next_pgc = {LinkNextPGC, 0, 0, 0};
    fprintf(stderr, "** Fell of the end of the pgc, continuing in NextPGC\n");
    assert((self->state).pgc->next_pgc_nr != 0);
    return link_next_pgc;
  }
}


static link_t process_command(vm_t *self, link_t link_values)
{
  /* FIXME $$$ Move this to a separate function? */
  self->badness_counter++;
  if (self->badness_counter > 1) fprintf(stderr, "**** process_command re-entered %d*****\n",self->badness_counter);
  while(link_values.command != PlayThis) {
    
    vmPrint_LINK(link_values);
    
    fprintf(stderr, "Link values %i %i %i %i\n", link_values.command, 
	    link_values.data1, link_values.data2, link_values.data3);
     
    fprintf(stderr, "Before:");
    vm_print_current_domain_state(self);
    
    switch(link_values.command) {
    case LinkNoLink:
      /* No Link */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      exit(1);
      
    case LinkTopC:
      /* Link to Top?? Cell */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      link_values = play_Cell(self);
      break;
    case LinkNextC:
      /* Link to Next Cell */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      (self->state).cellN += 1; /* FIXME: What if cellN becomes > nr_of_cells? */
      link_values = play_Cell(self);
      break;
    case LinkPrevC:
      /* Link to Previous Cell */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      (self->state).cellN -= 1; /*  FIXME: What if cellN becomes < 1? */
      link_values = play_Cell(self);
      break;
      
    case LinkTopPG:
      /* Link to Top Program */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      /*  Does pgN always contain the current value? */
      link_values = play_PG(self);
      break;
    case LinkNextPG:
      /* Link to Next Program */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      /*  Does pgN always contain the current value? */
      (self->state).pgN += 1; /*  FIXME: What if pgN becomes > pgc.nr_of_programs? */
      link_values = play_PG(self);
      break;
    case LinkPrevPG:
      /* Link to Previous Program */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      /*  Does pgN always contain the current value? */
      (self->state).pgN -= 1; /*  FIXME: What if pgN becomes < 1? */
      link_values = play_PG(self);
      break;
      
    case LinkTopPGC:
      /* Link to Top Program Chain */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      link_values = play_PGC(self);
      break;
    case LinkNextPGC:
      /* Link to Next Program Chain */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      assert((self->state).pgc->next_pgc_nr != 0);
      if(get_PGC(self, (self->state).pgc->next_pgc_nr))
	assert(0);
      link_values = play_PGC(self);
      break;
    case LinkPrevPGC:
      /* Link to Previous Program Chain */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      assert((self->state).pgc->prev_pgc_nr != 0);
      if(get_PGC(self, (self->state).pgc->prev_pgc_nr))
	assert(0);
      link_values = play_PGC(self);
      break;
    case LinkGoUpPGC:
      /* Link to GoUp??? Program Chain */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      assert((self->state).pgc->goup_pgc_nr != 0);
      if(get_PGC(self, (self->state).pgc->goup_pgc_nr))
	assert(0);
      link_values = play_PGC(self);
      break;
    case LinkTailPGC:
      /* Link to Tail??? Program Chain */
      if(link_values.data1 != 0)
	(self->state).HL_BTNN_REG = link_values.data1 << 10;
      link_values = play_PGC_post(self);
      break;
      
    case LinkRSM:
      {
	/* Link to Resume */
	int i;
	/*  Check and see if there is any rsm info!! */
	(self->state).domain = VTS_DOMAIN;
	ifoOpenNewVTSI(self, self->dvd, (self->state).rsm_vtsN);
	get_PGC(self, (self->state).rsm_pgcN);
	
	/* These should never be set in SystemSpace and/or MenuSpace */ 
	/* (self->state).TTN_REG = rsm_tt; ?? */
	/* (self->state).TT_PGCN_REG = (self->state).rsm_pgcN; ?? */
	for(i = 0; i < 5; i++) {
	  (self->state).registers.SPRM[4 + i] = (self->state).rsm_regs[i];
	}
	
	if(link_values.data1 != 0)
	  (self->state).HL_BTNN_REG = link_values.data1 << 10;
	
	if((self->state).rsm_cellN == 0) {
	  assert((self->state).cellN); /*  Checking if this ever happens */
	  /* assert( time/block/vobu is 0 ); */
	  (self->state).pgN = 1;
	  link_values = play_PG(self);
	} else { 
	  /* assert( time/block/vobu is _not_ 0 ); */
	  /* play_Cell_at_time */
	  /* (self->state).pgN = ?? this gets the righ value in play_Cell */
	  (self->state).cellN = (self->state).rsm_cellN;
	  link_values.command = PlayThis;
	  link_values.data1 = (self->state).rsm_blockN;
	  if(set_PGN(self)) {
	    /* Were at the end of the PGC, should not happen for a RSM */
	    assert(0);
	    link_values.command = LinkTailPGC;
	    link_values.data1 = 0;  /* No button */
	  }
	}
      }
      break;
    case LinkPGCN:
      /* Link to Program Chain Number:data1 */
      if(get_PGC(self, link_values.data1))
	assert(0);
      link_values = play_PGC(self);
      break;
    case LinkPTTN:
      /* Link to Part of this Title Number:data1 */
      assert((self->state).domain == VTS_DOMAIN);
      if(link_values.data2 != 0)
	(self->state).HL_BTNN_REG = link_values.data2 << 10;
      if(get_VTS_PTT(self, (self->state).vtsN, (self->state).VTS_TTN_REG, link_values.data1) == -1)
	assert(0);
      link_values = play_PG(self);
      break;
    case LinkPGN:
      /* Link to Program Number:data1 */
      if(link_values.data2 != 0)
	(self->state).HL_BTNN_REG = link_values.data2 << 10;
      /* Update any other state, PTTN perhaps? */
      (self->state).pgN = link_values.data1;
      link_values = play_PG(self);
      break;
    case LinkCN:
      /* Link to Cell Number:data1 */
      if(link_values.data2 != 0)
	(self->state).HL_BTNN_REG = link_values.data2 << 10;
      /* Update any other state, pgN, PTTN perhaps? */
      (self->state).cellN = link_values.data1;
      link_values = play_Cell(self);
      break;
      
    case Exit:
      exit(1); /*  What should we do here?? */
      
    case JumpTT:
      /* Jump to VTS Title Domain */
      /* Only allowed from the First Play domain(PGC) */
      /* or the Video Manager domain (VMG) */
      //fprintf(stderr,"****** JumpTT is Broken, please fix me!!! ****\n");
      assert((self->state).domain == VMGM_DOMAIN || (self->state).domain == FP_DOMAIN); /* ?? */
      if(get_TT(self,link_values.data1) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
    case JumpVTS_TT:
      /* Jump to Title:data1 in same VTS Title Domain */
      /* Only allowed from the VTS Menu Domain(VTSM) */
      /* or the Video Title Set Domain(VTS) */
      assert((self->state).domain == VTSM_DOMAIN || (self->state).domain == VTS_DOMAIN); /* ?? */
      /* FIXME: Should be able to use get_VTS_PTT here */
      if(get_VTS_TT(self,(self->state).vtsN, link_values.data1) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
    case JumpVTS_PTT:
      /* Jump to Part:data2 of Title:data1 in same VTS Title Domain */
      /* Only allowed from the VTS Menu Domain(VTSM) */
      /* or the Video Title Set Domain(VTS) */
      assert((self->state).domain == VTSM_DOMAIN || (self->state).domain == VTS_DOMAIN); /* ?? */
      if(get_VTS_PTT(self,(self->state).vtsN, link_values.data1, link_values.data2) == -1)
	assert(0);
      link_values = play_PG(self);
      break;
      
    case JumpSS_FP:
      /* Jump to First Play Domain */
      /* Only allowed from the VTS Menu Domain(VTSM) */
      /* or the Video Manager domain (VMG) */
      assert((self->state).domain == VMGM_DOMAIN || (self->state).domain == VTSM_DOMAIN); /* ?? */
      get_FP_PGC(self);
      link_values = play_PGC(self);
      break;
    case JumpSS_VMGM_MENU:
      /* Jump to Video Manger domain - Title Menu:data1 or any PGC in VMG */
      /* Allowed from anywhere except the VTS Title domain */
      assert((self->state).domain == VMGM_DOMAIN || 
	     (self->state).domain == VTSM_DOMAIN || 
	     (self->state).domain == FP_DOMAIN); /* ?? */
      (self->state).domain = VMGM_DOMAIN;
      if(get_MENU(self,link_values.data1) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
    case JumpSS_VTSM:
      /* Jump to a menu in Video Title domain, */
      /* or to a Menu is the current VTS */
      /* FIXME: This goes badly wrong for some DVDs. */
      /* FIXME: Keep in touch with ogle people regarding what to do here */
      fprintf(stderr, "dvdnav: BUG TRACKING *******************************************************************\n");
      fprintf(stderr, "dvdnav:              If you see this message, please report these values to the dvd-devel mailing list.\n");
      fprintf(stderr, "    data1=%u data2=%u data3=%u\n", 
                link_values.data1,
                link_values.data2,
                link_values.data3);
      fprintf(stderr, "dvdnav: *******************************************************************\n");

      if(link_values.data1 !=0) {
	assert((self->state).domain == VMGM_DOMAIN || (self->state).domain == FP_DOMAIN); /* ?? */
	(self->state).domain = VTSM_DOMAIN;
	ifoOpenNewVTSI(self, self->dvd, link_values.data1);  /*  Also sets (self->state).vtsN */
      } else {
	/*  This happens on 'The Fifth Element' region 2. */
	assert((self->state).domain == VTSM_DOMAIN);
      }
      /*  I don't know what title is supposed to be used for. */
      /*  Alien or Aliens has this != 1, I think. */
      /* assert(link_values.data2 == 1); */
      (self->state).VTS_TTN_REG = link_values.data2;
      if(get_MENU(self, link_values.data3) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
    case JumpSS_VMGM_PGC:
      assert((self->state).domain == VMGM_DOMAIN ||
	     (self->state).domain == VTSM_DOMAIN ||
	     (self->state).domain == FP_DOMAIN); /* ?? */
      (self->state).domain = VMGM_DOMAIN;
      if(get_PGC(self,link_values.data1) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
      
    case CallSS_FP:
      assert((self->state).domain == VTS_DOMAIN); /* ??    */
      /*  Must be called before domain is changed */
      saveRSMinfo(self, link_values.data1, /* We dont have block info */ 0);
      get_FP_PGC(self);
      link_values = play_PGC(self);
      break;
    case CallSS_VMGM_MENU:
      assert((self->state).domain == VTS_DOMAIN); /* ??    */
      /*  Must be called before domain is changed */
      saveRSMinfo(self,link_values.data2, /* We dont have block info */ 0);      
      (self->state).domain = VMGM_DOMAIN;
      if(get_MENU(self,link_values.data1) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
    case CallSS_VTSM:
      assert((self->state).domain == VTS_DOMAIN); /* ??    */
      /*  Must be called before domain is changed */
      saveRSMinfo(self,link_values.data2, /* We dont have block info */ 0);
      (self->state).domain = VTSM_DOMAIN;
      if(get_MENU(self,link_values.data1) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
    case CallSS_VMGM_PGC:
      assert((self->state).domain == VTS_DOMAIN); /* ??    */
      /*  Must be called before domain is changed */
      saveRSMinfo(self,link_values.data2, /* We dont have block info */ 0);
      (self->state).domain = VMGM_DOMAIN;
      if(get_PGC(self,link_values.data1) == -1)
	assert(0);
      link_values = play_PGC(self);
      break;
    case PlayThis:
      /* Should never happen. */
      break;
    }
  fprintf(stderr, "After:");
  vm_print_current_domain_state(self);
    
  }
  self->badness_counter--;
  return link_values;
}

static int get_TT(vm_t *self, int tt)
{  
  //fprintf(stderr,"****** get_TT is Broken, please fix me!!! ****\n");
  assert(tt <= self->vmgi->tt_srpt->nr_of_srpts);
  
  (self->state).TTN_REG = tt;
   
  return get_VTS_TT(self, self->vmgi->tt_srpt->title[tt - 1].title_set_nr,
		    self->vmgi->tt_srpt->title[tt - 1].vts_ttn);
}


static int get_VTS_TT(vm_t *self, int vtsN, int vts_ttn)
{
  int pgcN;
  //fprintf(stderr,"****** get_VTS_TT is Broken, please fix me!!! ****\n");
  fprintf(stderr,"title_set_nr=%d\n", vtsN);
  fprintf(stderr,"vts_ttn=%d\n", vts_ttn);
  
  (self->state).domain = VTS_DOMAIN;
  if(vtsN != (self->state).vtsN) {
    fprintf(stderr,"****** opening new VTSI ****\n");
    ifoOpenNewVTSI(self, self->dvd, vtsN); /*  Also sets (self->state).vtsN */
    fprintf(stderr,"****** opened VTSI ****\n");
  }
  
  pgcN = get_ID(self, vts_ttn); /*  This might return -1 */
  assert(pgcN != -1);

  /* (self->state).TTN_REG = ?? Must search tt_srpt for a matching entry...   */
  (self->state).VTS_TTN_REG = vts_ttn;
  /* Any other registers? */
  
  return get_PGC(self, pgcN);
}


static int get_VTS_PTT(vm_t *self, int vtsN, int /* is this really */ vts_ttn, int part)
{
  int pgcN, pgN;
  
  (self->state).domain = VTS_DOMAIN;
  if(vtsN != (self->state).vtsN)
    ifoOpenNewVTSI(self, self->dvd, vtsN); /*  Also sets (self->state).vtsN */
  
  assert(vts_ttn <= self->vtsi->vts_ptt_srpt->nr_of_srpts);
  assert(part <= self->vtsi->vts_ptt_srpt->title[vts_ttn - 1].nr_of_ptts);
  
  pgcN = self->vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgcn;
  pgN = self->vtsi->vts_ptt_srpt->title[vts_ttn - 1].ptt[part - 1].pgn;
  
  /* (self->state).TTN_REG = ?? Must search tt_srpt for a matchhing entry... */
  (self->state).VTS_TTN_REG = vts_ttn;
  /* Any other registers? */
  
  (self->state).pgN = pgN; /*  ?? */
  
  return get_PGC(self, pgcN);
}



static int get_FP_PGC(vm_t *self)
{  
  (self->state).domain = FP_DOMAIN;

  (self->state).pgc = self->vmgi->first_play_pgc;
  
  return 0;
}


static int get_MENU(vm_t *self, int menu)
{
  assert((self->state).domain == VMGM_DOMAIN || (self->state).domain == VTSM_DOMAIN);
  return get_PGC(self, get_ID(self, menu));
}

static int get_ID(vm_t *self, int id)
{
  int pgcN, i;
  pgcit_t *pgcit;
  
  /* Relies on state to get the correct pgcit. */
  pgcit = get_PGCIT(self);
  assert(pgcit != NULL);
  
  /* Get menu/title */
  for(i = 0; i < pgcit->nr_of_pgci_srp; i++) {
    if((pgcit->pgci_srp[i].entry_id & 0x7f) == id) {
      assert((pgcit->pgci_srp[i].entry_id & 0x80) == 0x80);
      pgcN = i + 1;
      return pgcN;
    }
  }
  fprintf(stderr, "** No such id/menu (%d) entry PGC\n", id);
  return -1; /*  error */
}



static int get_PGC(vm_t *self, int pgcN)
{
  /* FIXME: Keep this up to date with the ogle people */
  pgcit_t *pgcit;
  
  pgcit = get_PGCIT(self);
  
  assert(pgcit != NULL); /*  ?? Make this return -1 instead */
  if(pgcN < 1 || pgcN > pgcit->nr_of_pgci_srp) {
/*    if(pgcit->nr_of_pgci_srp != 1)  */
     return -1; /* error */
/*   pgcN = 1; */
  }
  
  /* (self->state).pgcN = pgcN; */
  (self->state).pgc = pgcit->pgci_srp[pgcN - 1].pgc;
  
  if((self->state).domain == VTS_DOMAIN)
    (self->state).TT_PGCN_REG = pgcN;

  return 0;
}

static int get_PGCN(vm_t *self)
{
  pgcit_t *pgcit;
  int pgcN = 1;

  pgcit = get_PGCIT(self);
  
  assert(pgcit != NULL);
  
  while(pgcN <= pgcit->nr_of_pgci_srp) {
    if(pgcit->pgci_srp[pgcN - 1].pgc == (self->state).pgc)
      return pgcN;
    pgcN++;
  }
  
  return -1; /*  error */
}


static int get_video_aspect(vm_t *self)
{
  int aspect = 0;
  
  if((self->state).domain == VTS_DOMAIN) {
    aspect = self->vtsi->vtsi_mat->vts_video_attr.display_aspect_ratio;  
  } else if((self->state).domain == VTSM_DOMAIN) {
    aspect = self->vtsi->vtsi_mat->vtsm_video_attr.display_aspect_ratio;
  } else if((self->state).domain == VMGM_DOMAIN) {
    aspect = self->vmgi->vmgi_mat->vmgm_video_attr.display_aspect_ratio;
  }
  fprintf(stderr, "dvdnav:get_video_aspect:aspect=%d\n",aspect);
  assert(aspect == 0 || aspect == 3);
  (self->state).registers.SPRM[14] &= ~(0x3 << 10);
  (self->state).registers.SPRM[14] |= aspect << 10;
  
  return aspect;
}






static void ifoOpenNewVTSI(vm_t *self, dvd_reader_t *dvd, int vtsN) 
{
  if((self->state).vtsN == vtsN) {
    return; /*  We alread have it */
  }
  
  if(self->vtsi != NULL)
    ifoClose(self->vtsi);
  
  self->vtsi = ifoOpenVTSI(dvd, vtsN);
  if(self->vtsi == NULL) {
    fprintf(stderr, "ifoOpenVTSI failed\n");
    exit(1);
  }
  if(!ifoRead_VTS_PTT_SRPT(self->vtsi)) {
    fprintf(stderr, "ifoRead_VTS_PTT_SRPT failed\n");
    exit(1);
  }
  if(!ifoRead_PGCIT(self->vtsi)) {
    fprintf(stderr, "ifoRead_PGCIT failed\n");
    exit(1);
  }
  if(!ifoRead_PGCI_UT(self->vtsi)) {
    fprintf(stderr, "ifoRead_PGCI_UT failed\n");
    exit(1);
  }
  if(!ifoRead_VOBU_ADMAP(self->vtsi)) {
    fprintf(stderr, "ifoRead_VOBU_ADMAP vtsi failed\n");
    exit(1);
  }
  if(!ifoRead_TITLE_VOBU_ADMAP(self->vtsi)) {
    fprintf(stderr, "ifoRead_TITLE_VOBU_ADMAP vtsi failed\n");
    exit(1);
  }
  (self->state).vtsN = vtsN;
}




static pgcit_t* get_MENU_PGCIT(vm_t *self, ifo_handle_t *h, uint16_t lang)
{
  int i;
  
  if(h == NULL || h->pgci_ut == NULL) {
    fprintf(stderr, "*** pgci_ut handle is NULL ***\n");
    return NULL; /*  error? */
  }
  
  i = 0;
  while(i < h->pgci_ut->nr_of_lus
	&& h->pgci_ut->lu[i].lang_code != lang)
    i++;
  if(i == h->pgci_ut->nr_of_lus) {
    fprintf(stderr, "Language '%c%c' not found, using '%c%c' instead\n",
	    (char)(lang >> 8), (char)(lang & 0xff),
 	    (char)(h->pgci_ut->lu[0].lang_code >> 8),
	    (char)(h->pgci_ut->lu[0].lang_code & 0xff));
    i = 0; /*  error? */
  }
  
  return h->pgci_ut->lu[i].pgcit;
}

/* Uses state to decide what to return */
static pgcit_t* get_PGCIT(vm_t *self) {
  pgcit_t *pgcit;
  
  if((self->state).domain == VTS_DOMAIN) {
    pgcit = self->vtsi->vts_pgcit;
  } else if((self->state).domain == VTSM_DOMAIN) {
    pgcit = get_MENU_PGCIT(self, self->vtsi, (self->state).registers.SPRM[0]);
  } else if((self->state).domain == VMGM_DOMAIN) {
    pgcit = get_MENU_PGCIT(self, self->vmgi, (self->state).registers.SPRM[0]);
  } else {
    pgcit = NULL;    /* Should never hapen */
  }
  
  return pgcit;
}

/*
 * $Log$
 * Revision 1.3  2002/04/02 18:22:27  richwareham
 * Added reset patch from Kees Cook <kees@outflux.net>
 *
 * Revision 1.2  2002/04/01 18:56:28  richwareham
 * Added initial example programs directory and make sure all debug/error output goes to stderr.
 *
 * Revision 1.1.1.1  2002/03/12 19:45:55  richwareham
 * Initial import
 *
 * Revision 1.18  2002/01/22 16:56:49  jcdutton
 * Fix clut after seeking.
 * Add a few virtual machine debug messages, to help diagnose problems with "Deep Purple - Total Abandon" DVD as I don't have the DVD itself.
 * Fix a few debug messages, so they do not say FIXME.
 * Move the FIXME debug messages to comments in the code.
 *
 * Revision 1.17  2002/01/21 01:16:30  jcdutton
 * Added some debug messages, to hopefully get info from users.
 *
 * Revision 1.16  2002/01/20 21:40:46  jcdutton
 * Start to fix some assert failures.
 *
 * Revision 1.15  2002/01/19 20:24:38  jcdutton
 * Just some FIXME notes added.
 *
 * Revision 1.14  2002/01/13 22:17:57  jcdutton
 * Change logging.
 *
 *
 */
