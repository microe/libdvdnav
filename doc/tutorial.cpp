/*! \page tutorial A libdvdnav Tutorial

The <tt>libdvdnav</tt> library provides a powerful API allowing your
programs to take advantage of the sophisticated navigation features
on DVDs. 

\subsection wherenow Tutorial sections

  - For an introduction to the navigation features of DVDs look in section
    \ref dvdnavissues . This also provides an overview of the concepts
    required to understand DVD navigation.
  - For a step-by step walkthrough of a simple program look in
    section \ref firstprog .
  - FIXME: More sections :)
  
*/

/*! \page dvdnavissues An introduction to DVD navigation

The DVD format represents a radical departure from the traditional
form of video home-entertainment. Instead of just being a linear 
programme which is watched from beginning to end like a novel
DVD allows the user to jump about at will (much like those
'Choose your own adventure' or 'Which Way' books which were
popular a while back).

Such features are usually referred to under the moniker
'interactive' by marketting people but you aren't in marketting
since you are reading the <tt>libdvdnav</tt> tutorial. We'll
assume you actually want to know precisely what DVD can do.

\subsection layout DVD logical layout

A DVD is logically structured into titles, chapters (also
known as 'parts'), cells and VOBUS, much like the
filesystem on your hard disc. The structure is heirachical.
A typical DVD might have the following structure:

\verbatim
  .
  |-- Title 1
  |   |-- Chapter 1
  |   |   |-- Cell 1
  |   |   |   |-- VOBU 1
  |   |   |   |-- ... 
  |   |   |   `-- VOBU n
  |   |   |-- ...
  |   |   `-- Cell n
  |   |-- ...
  |   `-- Chapter 2
  |       |-- Cell 1
  |       |   |-- VOBU 1
  |       |   |-- ... 
  |       |   `-- VOBU n
  |       |-- ...
  |       `-- Cell n
  |-- ...
  `-- Title m
      |-- Chapter 1
      |   |-- Cell 1
      |   |   |-- VOBU 1
      |   |   |-- ... 
      |   |   `-- VOBU n
      |   |-- ...
      |   `-- Cell n
      |-- ...
      `-- Chapter 2
          |-- Cell 1
          |   |-- VOBU 1
          |   |-- ... 
          |   `-- VOBU n
          |-- ...
          `-- Cell n
\endverbatim

A DVD 'Title' is generally a logically distinct section of video. For example the main
feature film on a DVD might be Title 1, a behind-the-scenes documentary might be Title 2
and a selection of cast interviews might be Title 3. There can be up to 99 Titles on
any DVD.

A DVD 'Chapter' (somewhat confusingly referred to as a 'Part' in the parlence of
DVD authors) is generally a logical segment of a Title such as a scene in a film
or one interview in a set of cast interviews. There can be up to 999 Parts in
one Title.

A 'Cell' is a small segment of a Part. It is the smallest resolution at which
DVD navigation commands can act (e.g. 'Jump to Cell 3 of Part 4 of Title 2').
Typically one Part contains one Cell but on complex DVDs it may be useful to have
multiple Cells per Part.

A VOBU (<I>V</I>ideo <I>OB</I>ject <I>U</I>nit) is a small (typically a few
seconds) of video. It must be a self contained 'Group of Pictures' which
can be understood by the MPEG decoder. All seeking, jumping, etc is guaranteed
to occurr at a VOBU boundary so that the decoder need not be restarted and that
the location jumped to is always the start of a valid MPEG stream. For multiple-angle
DVDs VOBUs for each angle can be interleaved into one Interleaved Video Unit (ILVU).
In this case when the player get to the end of the VOBU for angle <i>n</i> instead of
jumping to the next VOBU the player will move forward to the VOBU for angle <i>n</i>
in the next ILVU. 

This is summarised in the following diagram showing how the VOBUs are actually
laid out on disc.

\verbatim
  ,---------------------------.     ,---------------------------.
  | ILVU 1                    |     | ILVU m                    |
  | ,--------.     ,--------. |     | ,--------.     ,--------. |
  | | VOBU 1 | ... | VOBU 1 | | ... | | VOBU m | ... | VOBU m | |
  | |Angle 1 |     |Angle n | |     | |Angle 1 |     |Angle n | |
  | `--------'     `--------' |     | `--------'     `--------' |
  `---------------------------'     `---------------------------'
\endverbatim

\subsection vm The DVD Virtual Machine

If the layout of the DVD were the only feature of the format the DVD
would only have a limited amount of interactivity, you could jump
around between Titles, Parts and Cells but not much else.

The feature most people associate with DVDs is its ability to 
present the user with full-motion interactive menus. To provide
these features the DVD format includes a specification for a 
DVD 'virtual machine'.

To a first order approximation x86 programs can only be run on
x86-based machines, PowerPC programs on PowerPC-based machines and so on.
Java, however, is an exception in that programs are compiled into
a special code which is designed for a 'Java Virtual Machine'.
Programmes exist which take this code and convert it into code which
can run on real processors.

Similarly the DVD virtual machine is a hypothetical processor
which has commands useful for DVD navigation (e.g. Jump to Title
4 or Jump to Cell 2) along with the ability to perform
simple arithmetic and save values in a number of special
variables (in processor speak, they are known as 'registers').

When a button is pressed on a DVD menu, a specified machine instruction
can be executed (e.g. to jump to a particular Title). Similarly
commands can be executed at the beginning and end of Cells and
Parts to, for example, return to the menu at the end of a film.

Return to the \ref tutorial.

*/

/*! \page firstprog A first libdvdnav program

\verbatim
int main(int argc, char **argv) {
  
  printf("Opening DVD...\n");
  dvdnav_open(&dvdnav, "/dev/dvd");

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
\endverbatim

Return to the \ref tutorial.

*/
