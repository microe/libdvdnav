#include "../src/dvdnav.h"
#include <stdio.h>
#include <dvdread/nav_read.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define CALL(x) if(x != DVDNAV_STATUS_OK) { \
  fprintf(stderr, "Error calling '%s' (%s)\n", \
          #x, dvdnav_err_to_string(dvdnav)); \
  exit(0); \
}

dvdnav_t *dvdnav;
char buf[2050];

/**
 * Returns 1 if block contains NAV packet, 0 otherwise.
 * Puts pci and dsi packets into appropriate parameters if present
 * 
 * Most of the code in here is copied from xine's MPEG demuxer
 * so any bugs which are found in that should be corrected here also.
 */
int check_packet(uint8_t *p, pci_t* pci, dsi_t* dsi) {
  int            bMpeg1=0;
  uint32_t       nHeaderLen;
  uint32_t       nPacketLen;
  uint32_t       nStreamID;
/* uint8_t       *p_start=p; */


  if (p==NULL) {
    fprintf(stderr,"Passed a NULL pointer.\n");
    return 0;
  }

  /* dprint("Checking packet...\n"); */

  if (p[3] == 0xBA) { /* program stream pack header */

    int nStuffingBytes;

    /* xprintf (VERBOSE|DEMUX, "program stream pack header\n"); */

    bMpeg1 = (p[4] & 0x40) == 0;

    if (bMpeg1) {
      p   += 12;
    } else { /* mpeg2 */
      nStuffingBytes = p[0xD] & 0x07;
      p += 14 + nStuffingBytes;
    }
  }


  if (p[3] == 0xbb) { /* program stream system header */
    int nHeaderLen;

    nHeaderLen = (p[4] << 8) | p[5];
    p += 6 + nHeaderLen;
  }

  /* we should now have a PES packet here */

  if (p[0] || p[1] || (p[2] != 1)) {
    fprintf(stderr,"demux error! %02x %02x %02x (should be 0x000001) \n",p[0],p[1],p[2]);
    return 0;
  }

  nPacketLen = p[4] << 8 | p[5];
  nStreamID  = p[3];

  nHeaderLen = 6;
  p += nHeaderLen;

  if (nStreamID == 0xbf) { /* Private stream 2 */
/*
 *   int i;
 *    printf("dvdnav:nav packet=%u\n",p-p_start-6);
 *   for(i=0;i<80;i++) {
 *     printf("%02x ",p[i-6]);
 *   }
 *   printf("\n");
 */
    if(p[0] == 0x00) {
      navRead_PCI(pci, p+1);
    }

    p += nPacketLen;

    /* We should now have a DSI packet. */
    if(p[6] == 0x01) {
      int num=0, current=0;

      nPacketLen = p[4] << 8 | p[5];
      p += 6;
      /* dprint("NAV DSI packet\n");  */
      navRead_DSI(dsi, p+1);
    }
    return 1;
  }

  return 0;
}

int main(int argc, char **argv) {
  int region, i, finished;
  int event, len, dump;
  int output_fd;
  
  printf("Opening output...\n");
  output_fd = open("out.vob", O_CREAT | O_WRONLY);
  if(output_fd == -1) {
    printf("Error opening output\n");
    exit(1);
  }
  
  printf("Opening DVD...\n");
  CALL(dvdnav_open(&dvdnav, "/dev/dvd"));

  region = 0; dump = 0;
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
      exit(1);
    }

    switch(event) {
     case DVDNAV_BLOCK_OK:
      if(dump) {
	write(output_fd, buf, len);
      }
      break;
     case DVDNAV_NOP:
      /* Nothing */
      break;
     case DVDNAV_STILL_FRAME: 
       {
	dvdnav_still_event_t *still_event = (dvdnav_still_event_t*)(buf);

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

	dump = 0;
	ch = 0;

	fflush(stdin);
	while((ch != 'a') && (ch != 's') && (ch != 'q')) {
	  printf("(a)ppend cell to output/(s)kip cell/(q)uit? (a/s/q): ");
	  scanf("%c", &ch);
	}

	if(ch == 'a') {
	  dump = 1;
	}
	if(ch == 'q') {
	  finished = 1;
	}
       }
      break;
     case DVDNAV_SEEK_DONE:
       {
       }
      break;
     case DVDNAV_NAV_PACKET:
       {
	pci_t pci;
	dsi_t dsi;
	
	/* Take a look at the NAV packet to see if there
	 * is a menu */
	if(check_packet(buf, &pci, &dsi)) {
	  if(pci.hli.hl_gi.btn_ns > 0) {
	    int button;
	    
	    printf("Found %i buttons...\n", pci.hli.hl_gi.btn_ns);

	    for(button = 0; button<pci.hli.hl_gi.btn_ns; button++) {
	      btni_t *btni;

	      btni = &(pci.hli.btnit[button]);
	      printf("Button %i top-left @ (%i,%i), bottom-right @ (%i,%i)\n", 
		     button+1, btni->x_start, btni->y_start,
		     btni->x_end, btni->y_end);
	      
	    }

	    button = 0;
	    while((button <= 0) || (button > pci.hli.hl_gi.btn_ns)) {
	      printf("Which button (1 to %i): ", pci.hli.hl_gi.btn_ns);
	      scanf("%i", &button);
	    }

	    printf("Selecting button %i\n", button);
	    dvdnav_button_select_and_activate(dvdnav, button);
	  }
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
