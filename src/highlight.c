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
#include <assert.h>

#include <dvdnav.h>
#include "dvdnav_internal.h"

#include "vm.h"
#
#include <dvdread/nav_types.h>

#ifdef BUTTON_TESTING
#include <dvdread/nav_print.h>
#endif

/* Highlighting API calls */

dvdnav_status_t dvdnav_get_current_highlight(dvdnav_t *this, int* button) {
  if(!this)
   return S_ERR;

  /* Simply return the appropriate value based on the SPRM */
  (*button) = (this->vm->state.HL_BTNN_REG) >> 10;
  
  return S_OK;
}

pci_t* dvdnav_get_current_nav_pci(dvdnav_t *this) {
  if (!this ) assert(0);
  return &this->pci;
}

btni_t *__get_current_button(dvdnav_t *this) {
  int button = 0;

  if(dvdnav_get_current_highlight(this, &button) != S_OK) {
    printerrf("Unable to get information on current highlight.");
    return NULL;
  }
#ifdef BUTTON_TESTING
  navPrint_PCI(&(this->pci));
#endif
  
  return &(this->pci.hli.btnit[button-1]);
}

dvdnav_status_t dvdnav_upper_button_select(dvdnav_t *this) {
  btni_t *button_ptr;
  
  if(!this)
   return S_ERR;

  if((button_ptr = __get_current_button(this)) == NULL) {
    return S_ERR;
  }

  dvdnav_button_select(this, button_ptr->up);
  
  return S_OK;
}

dvdnav_status_t dvdnav_lower_button_select(dvdnav_t *this) {
  btni_t *button_ptr;
  
  if(!this)
   return S_ERR;

  if((button_ptr = __get_current_button(this)) == NULL) {
    return S_ERR;
  }

  dvdnav_button_select(this, button_ptr->down);
  
  return S_OK;
}

dvdnav_status_t dvdnav_right_button_select(dvdnav_t *this) {
  btni_t *button_ptr;
  
  if(!this)
   return S_ERR;

  if((button_ptr = __get_current_button(this)) == NULL) {
    printerr("Error fetching information on current button.");
    return S_ERR;
  }

  dvdnav_button_select(this, button_ptr->right);
  
  return S_OK;
}

dvdnav_status_t dvdnav_left_button_select(dvdnav_t *this) {
  btni_t *button_ptr;
  
  if(!this)
   return S_ERR;

  if((button_ptr = __get_current_button(this)) == NULL) {
    return S_ERR;
  }

  dvdnav_button_select(this, button_ptr->left);
  
  return S_OK;
}

dvdnav_status_t dvdnav_get_highlight_area(pci_t* nav_pci , int32_t button, int32_t mode, 
                                           dvdnav_highlight_area_t* highlight) {
  btni_t *button_ptr;
  fprintf(stderr,"Button get_highlight_area %i\n", button);

  /* Set the highlight SPRM if the passed button was valid*/
  if((button <= 0) || (button > nav_pci->hli.hl_gi.btn_ns)) {
    fprintf(stderr,"Unable to select button number %i as it doesn't exist",
              button);
    return S_ERR;
  }
  button_ptr = &nav_pci->hli.btnit[button-1];

  highlight->sx = button_ptr->x_start;
  highlight->sy = button_ptr->y_start;
  highlight->ex = button_ptr->x_end;
  highlight->ey = button_ptr->y_end;
  if(button_ptr->btn_coln != 0) {
    highlight->palette = nav_pci->hli.btn_colit.btn_coli[button_ptr->btn_coln-1][mode];
  } else {
    highlight->palette = 0;
  }
  highlight->pts = nav_pci->hli.hl_gi.hli_s_ptm;
  highlight->buttonN = button;
//#ifdef BUTTON_TESTING
  fprintf(stderr,"highlight.c:Highlight area is (%u,%u)-(%u,%u), display = %i, button = %u\n",
               button_ptr->x_start, button_ptr->y_start,
               button_ptr->x_end, button_ptr->y_end,
               1,
               button);
//#endif

  return S_OK;
}

dvdnav_status_t dvdnav_button_activate(dvdnav_t *this) {
  int button;
  btni_t *button_ptr = NULL;
  
  if(!this) 
   return S_ERR;
  pthread_mutex_lock(&this->vm_lock); 

  /* Precisely the same as selecting a button except we want
   * a different palette */
  if(dvdnav_get_current_highlight(this, &button) != S_OK) {
    pthread_mutex_unlock(&this->vm_lock); 
    return S_ERR;
  }
  if(dvdnav_button_select(this, button) != S_OK) {
    pthread_mutex_unlock(&this->vm_lock); 
    return S_ERR;
  }
  /* FIXME: The button command should really be passed in the API instead. */ 
  button_ptr = __get_current_button(this);
  /* Finally, make the VM execute the appropriate code and
   * scedule a jump */
  fprintf(stderr, "libdvdnav: Evaluating Button Activation commands.\n");
  if(vm_eval_cmd(this->vm, &(button_ptr->cmd)) == 1) {
    /* Command caused a jump */
    this->vm->hop_channel++;
    this->position_current.still = 0;
  }
  pthread_mutex_unlock(&this->vm_lock); 
  return S_OK;
}

dvdnav_status_t dvdnav_button_select(dvdnav_t *this, int button) {
  
  if(!this) {
   printerrf("Unable to select button number %i as this state bad",
	      button);
   return S_ERR;
  }
 
  fprintf(stderr,"Button select %i\n", button); 
  
  /* Set the highlight SPRM if the passed button was valid*/
  if((button <= 0) || (button > this->pci.hli.hl_gi.btn_ns)) {
    printerrf("Unable to select button number %i as it doesn't exist",
	      button);
    return S_ERR;
  }
  this->vm->state.HL_BTNN_REG = (button << 10);

  this->hli_state = 1; /* Selected */

//  this->position_current.button = -1; /* Force Highligh change */

  return S_OK;
}

dvdnav_status_t dvdnav_button_select_and_activate(dvdnav_t *this, 
						  int button) {
  /* A trivial function */
  if(dvdnav_button_select(this, button) != S_ERR) {
    return dvdnav_button_activate(this);
  }
  
  /* Should never get here without an error */
  return S_ERR;
}

dvdnav_status_t dvdnav_mouse_select(dvdnav_t *this, int x, int y) {
  int button, cur_button;
  
  /* FIXME: At the moment, the case of no button matchin (x,y) is
   * silently ignored, is this OK? */
  if(!this)
   return S_ERR;

  if(dvdnav_get_current_highlight(this, &cur_button) != S_OK) {
    return S_ERR;
  }

  /* Loop through each button */
  for(button=1; button <= this->pci.hli.hl_gi.btn_ns; button++) {
    btni_t *button_ptr = NULL;

    button_ptr = &(this->pci.hli.btnit[button-1]);
    if((x >= button_ptr->x_start) && (x <= button_ptr->x_end) &&
       (y >= button_ptr->y_start) && (y <= button_ptr->y_end)) {
      /* As an efficiency measure, only re-select the button
       * if it is different to the previously selected one. */
      if(button != cur_button) {
	dvdnav_button_select(this, button);
      }
    }
  }
  
  return S_OK;
}

dvdnav_status_t dvdnav_mouse_activate(dvdnav_t *this, int x, int y) {
  /* A trivial function */
  if(dvdnav_mouse_select(this, x,y) != S_ERR) {
    return dvdnav_button_activate(this);
  }
  
  /* Should never get here without an error */
  return S_ERR;
}

