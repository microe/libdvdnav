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

/*
#define BUTTON_TESTING
*/

#include <dvdnav.h>
#include "dvdnav_internal.h"

#include "vm.h"
#
#include <dvdread/nav_types.h>

#ifdef BUTTON_TESTING
#include <dvdread/nav_print.h>
#endif

/* Highlighting API calls */

dvdnav_status_t dvdnav_get_current_highlight(dvdnav_t *self, int* button) {
  if(!self)
   return S_ERR;

  /* Simply return the appropriate value based on the SPRM */
  (*button) = (self->vm->state.HL_BTNN_REG) >> 10;
  
  return S_OK;
}

btni_t *__get_current_button(dvdnav_t *self) {
  int button = 0;

  if(dvdnav_get_current_highlight(self, &button) != S_OK) {
    printerrf("Unable to get information on current highlight.");
    return NULL;
  }
#ifdef BUTTON_TESTING
  navPrint_PCI(&(self->pci));
#endif
  
  return &(self->pci.hli.btnit[button-1]);
}

dvdnav_status_t dvdnav_upper_button_select(dvdnav_t *self) {
  btni_t *button_ptr;
  
  if(!self)
   return S_ERR;

  if((button_ptr = __get_current_button(self)) == NULL) {
    return S_ERR;
  }

  dvdnav_button_select(self, button_ptr->up);
  
  return S_OK;
}

dvdnav_status_t dvdnav_lower_button_select(dvdnav_t *self) {
  btni_t *button_ptr;
  
  if(!self)
   return S_ERR;

  if((button_ptr = __get_current_button(self)) == NULL) {
    return S_ERR;
  }

  dvdnav_button_select(self, button_ptr->down);
  
  return S_OK;
}

dvdnav_status_t dvdnav_right_button_select(dvdnav_t *self) {
  btni_t *button_ptr;
  
  if(!self)
   return S_ERR;

  if((button_ptr = __get_current_button(self)) == NULL) {
    printerr("Error fetching information on current button.");
    return S_ERR;
  }

  dvdnav_button_select(self, button_ptr->right);
  
  return S_OK;
}

dvdnav_status_t dvdnav_left_button_select(dvdnav_t *self) {
  btni_t *button_ptr;
  
  if(!self)
   return S_ERR;

  if((button_ptr = __get_current_button(self)) == NULL) {
    return S_ERR;
  }

  dvdnav_button_select(self, button_ptr->left);
  
  return S_OK;
}

dvdnav_status_t dvdnav_button_activate(dvdnav_t *self) {
  int button;
  btni_t *button_ptr;
  
  if(!self) 
   return S_ERR;
  pthread_mutex_lock(&self->vm_lock); 

  /* Precisely the same as selecting a button except we want
   * a different palette */
  if(dvdnav_get_current_highlight(self, &button) != S_OK) {
    pthread_mutex_unlock(&self->vm_lock); 
    return S_ERR;
  }
  if(dvdnav_button_select(self, button) != S_OK) {
    pthread_mutex_unlock(&self->vm_lock); 
    return S_ERR;
  }
  
  /* Now get the current button's info */
  if((button_ptr = __get_current_button(self)) == NULL) {
    printerr("Error fetching information on current button.");
    pthread_mutex_unlock(&self->vm_lock); 
    return S_ERR;
  }

  /* And set the palette */
  if(button_ptr->btn_coln != 0) {
    self->hli_clut = 
     self->pci.hli.btn_colit.btn_coli[button_ptr->btn_coln-1][1];
  } else {
    self->hli_clut = 0;
  }

  /* Finally, make the VM execute the appropriate code and
   * scedule a jump */

  if(vm_eval_cmd(self->vm, &(button_ptr->cmd)) == 1) {
    /* Cammand caused a jump */
    dvdnav_do_post_jump(self);
  }
  
  pthread_mutex_unlock(&self->vm_lock); 
  return S_OK;
}

dvdnav_status_t dvdnav_button_select(dvdnav_t *self, int button) {
  btni_t *button_ptr;
  
  if(!self) {
   printerrf("Unable to select button number %i as self state bad",
	      button);
   return S_ERR;
  }
 
  printf("Button select %i\n", button); 
  
  /* Set the highlight SPRM if the passed button was valid*/
  if((button <= 0) || (button > self->pci.hli.hl_gi.btn_ns)) {
    printerrf("Unable to select button number %i as it doesn't exist",
	      button);
    return S_ERR;
  }
  self->vm->state.HL_BTNN_REG = (button << 10);

  /* Now get the current button's info */
  if((button_ptr = __get_current_button(self)) == NULL) {
    printerr("Error fetching information on current button.");
    return S_ERR;
  }

  self->hli_bbox[0] = button_ptr->x_start;
  self->hli_bbox[1] = button_ptr->y_start;
  self->hli_bbox[2] = button_ptr->x_end;
  self->hli_bbox[3] = button_ptr->y_end;
  self->hli_state = 1; /* Selected */

  if(button_ptr->btn_coln != 0) {
    self->hli_clut = 
     self->pci.hli.btn_colit.btn_coli[button_ptr->btn_coln-1][0];
  } else {
    self->hli_clut = 0;
  }
  self->hli_pts = self->pci.hli.hl_gi.hli_s_ptm;
  self->hli_buttonN = button;
  self->highlight_changed = 1;
#ifdef BUTTON_TESTING
  printf("highlight.c:Highlight area is (%u,%u)-(%u,%u), display = %i, button = %u\n",
	       button_ptr->x_start, button_ptr->y_start,
	       button_ptr->x_end, button_ptr->y_end,
	       1,
	       button);
#endif

  return S_OK;
}

dvdnav_status_t dvdnav_button_select_and_activate(dvdnav_t *self, 
						  int button) {
  /* A trivial function */
  if(dvdnav_button_select(self, button) != S_ERR) {
    return dvdnav_button_activate(self);
  }
  
  /* Should never get here without an error */
  return S_ERR;
}

dvdnav_status_t dvdnav_mouse_select(dvdnav_t *self, int x, int y) {
  int button, cur_button;
  
  /* FIXME: At the moment, the case of no button matchin (x,y) is
   * silently ignored, is this OK? */
  if(!self)
   return S_ERR;

  if(dvdnav_get_current_highlight(self, &cur_button) != S_OK) {
    return S_ERR;
  }

  /* Loop through each button */
  for(button=1; button <= self->pci.hli.hl_gi.btn_ns; button++) {
    btni_t *button_ptr = NULL;

    button_ptr = &(self->pci.hli.btnit[button-1]);
    if((x >= button_ptr->x_start) && (x <= button_ptr->x_end) &&
       (y >= button_ptr->y_start) && (y <= button_ptr->y_end)) {
      /* As an efficiency measure, only re-select the button
       * if it is different to the previously selected one. */
      if(button != cur_button) {
	dvdnav_button_select(self, button);
      }
    }
  }
  
  return S_OK;
}

dvdnav_status_t dvdnav_mouse_activate(dvdnav_t *self, int x, int y) {
  /* A trivial function */
  if(dvdnav_mouse_select(self, x,y) != S_ERR) {
    return dvdnav_button_activate(self);
  }
  
  /* Should never get here without an error */
  return S_ERR;
}

