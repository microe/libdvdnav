dvdnav_t *dvdnav;
int region_mask, result;
int event, len;
unsigned char buf[2048];

if(dvdnav_open(&dvdnav, "/dev/dvd") != DVDNAV_STATUS_OK) {
  /* Report failure */
  fprintf(stderr, "Could not open DVD\n");
  exit(1);
}

dvdnav_get_region_mask(dvdnav, &region_mask);
printf("Region mask: 0x%02x\n", region_mask);

do {
  result = dvdnav_get_next_block(dvdnav, buf,
	  			 &event, &len);

  /* Process the buffer */

} while(result == DVDNAV_STATUS_OK);

dvdnav_close(dvdnav);
