#include "dvdnav.h"
#include <stdio.h>
#include <dvdread/nav_read.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#define CALL(x) if(x != DVDNAV_STATUS_OK) { \
  fprintf(stderr, "Error calling '%s' (%s)\n", \
          #x, dvdnav_err_to_string(dvdnav)); \
  assert(0); \
}

dvdnav_t *dvdnav;
char buf[2050];

int main(int argc, char **argv) {
  int region, i, finished;
  int event, len, dump, tt_dump;
  int output_fd, output_no;
  unsigned long output_size;
  char filename[256];
  int change_output;
  
  output_no = 0; output_size = 0;
  change_output = 0; output_fd = -1;
  printf("Opening DVD...\n");
  CALL(dvdnav_open(&dvdnav, "/dev/dvd"));

  region = 0; dump = 0; tt_dump = -2;
  CALL(dvdnav_get_region_mask(dvdnav, &region));
  printf("The DVD can be played in the following regions: ");
  for(i=0; i<8; i++) {
    if(region & (1<<i)) {
      printf("%i ", i+1);
    }
  }
  printf("\n");

  printf("Reading...\n");
  finished = 0;
  while(!finished) {
    int result = dvdnav_get_next_block(dvdnav, buf,
				       &event, &len);

    if(result == DVDNAV_STATUS_ERR) {
      fprintf(stderr, "Error getting next block (%s)\n",
	      dvdnav_err_to_string(dvdnav));
      assert(0);
    }

    switch(event) {
     case DVDNAV_BLOCK_OK:
      
      if((output_no == 0) || (change_output)) {
	output_no++;
	output_size = 0;
	change_output = 0;
	printf("Opening output...\n");
	snprintf(filename, 255, "out_%03i.mpeg", output_no);
	if(output_fd != -1) {
	  close(output_fd);
	}
	output_fd = open(filename, O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG);
	if(output_fd == -1) {
	  printf("Error opening output\n");
	  assert(0);
	}
      }
      
      if(dump || (tt_dump != -2)) {
	write(output_fd, buf, len);
	output_size += (unsigned long)len;
      }
      break;
     case DVDNAV_NOP:
      /* Nothing */
      break;
     case DVDNAV_STILL_FRAME: 
       {
	printf("Skipping still frame\n");
	dvdnav_still_skip(dvdnav);
       }
      break;
     case DVDNAV_SPU_CLUT_CHANGE:
       {
       }
      break;
     case DVDNAV_SPU_STREAM_CHANGE:
       {
       }
      break;
     case DVDNAV_AUDIO_STREAM_CHANGE:
       {
       }
      break;
     case DVDNAV_HIGHLIGHT:
       {
       }
      break;
     case DVDNAV_VTS_CHANGE:
       {
       }
      break;
     case DVDNAV_CELL_CHANGE:
       {
	int tt,pt;
	char ch;
	
	dvdnav_current_title_info(dvdnav, &tt, &pt);
	printf("Cell change... (title, chapter) = (%i,%i)\n", tt,pt);

	if(output_size > (unsigned long)1073741824) {
	  /* If written more than a gig */
	  change_output = 1;
	}

	if(tt_dump != -2) {
	  if(tt != tt_dump) {
	    tt_dump = -2;
	    dump = 0;
	  } else {
	    printf("Appending as still title %i\n",tt);
	  }
	} else {
	  dump = 0;
	  ch = 0;
	  
	  fflush(stdin);
	  while((ch != 'a') && (ch != 's') && (ch != 'q') && (ch != 't')) {
	    printf("(a)ppend cell to output/(s)kip cell/append until end of (t)itle/(q)uit? (a/s/t/q): ");
	    scanf("%c", &ch);
	  }
	  
	  if(ch == 'a') {
	    dump = 1;
	  }
	  if(ch == 'q') {
	    finished = 1;
	  }
	  if(ch == 't') {
	    tt_dump = tt;
	  }
	}
       }
      break;
     case DVDNAV_SEEK_DONE:
       {
       }
      break;
     case DVDNAV_NAV_PACKET:
       {
	pci_t *pci;
	dsi_t *dsi;
	
	/* Take a look at PCI/DSI to see if there is a menu */
	pci = dvdnav_get_current_nav_pci(dvdnav);
	dsi = dvdnav_get_current_nav_dsi(dvdnav);
	
	if(pci->hli.hl_gi.btn_ns > 0) {
	  int button;
	    
	  printf("Found %i buttons...\n", pci->hli.hl_gi.btn_ns);

	  for(button = 0; button<pci->hli.hl_gi.btn_ns; button++) {
	    btni_t *btni;
            btni = &(pci->hli.btnit[button]);
	    printf("Button %i top-left @ (%i,%i), bottom-right @ (%i,%i)\n", 
	            button+1, btni->x_start, btni->y_start,
		    btni->x_end, btni->y_end);
	  }

	  button = 0;
	  while((button <= 0) || (button > pci->hli.hl_gi.btn_ns)) {
	    printf("Which button (1 to %i): ", pci->hli.hl_gi.btn_ns);
	    scanf("%i", &button);
	  }

	  printf("Selecting button %i\n", button);
	  dvdnav_button_select_and_activate(dvdnav, pci, button);
	}
      }
      break;
     case DVDNAV_STOP:
       {
	finished = 1;
       }
     default:
      printf("Unknown event (%i)\n", event);
      finished = 1;
      break;
    }
  }
  
  CALL(dvdnav_close(dvdnav));
  close(output_fd);
  
  return 0;
} 
