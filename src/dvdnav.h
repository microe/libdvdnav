/* 
 * Copyright (C) 2001 Rich Wareham <richwareham@users.sourceforge.net>
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

#ifndef DVDNAV_H_INCLUDED
#define DVDNAV_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* Defines the various events and dvdnav_event_t */
#include "dvdnav_events.h"

/* Various useful types */
#include "dvd_types.h"

#include <dvdread/dvd_reader.h>

/* Opaque dvdnav data-type */
typedef struct dvdnav_s dvdnav_t;

/* Status */
typedef int dvdnav_status_t;

#define DVDNAV_STATUS_ERR 0
#define DVDNAV_STATUS_OK  1

/**
 * NOTE: Unless otherwise stated, all functions return DVDNAV_STATUS_OK if
 * they succeeded, otherwise DVDNAV_STATUS_ERR is returned and the error may
 * be obtained by calling dvdnav_err_to_string().
 */

/*** Initialisation & housekeeping functions ***/

/**
 * Attempts to open the DVD drive at the specifiec path and pre-cache
 * any CSS-keys that your hacked libdvdread may use.
 *
 * Arguments:
 *   dest     -- Pointer to a dvdnav_t pointer to fill in.
 *   path     -- Any libdvdread acceptable path
 */
dvdnav_status_t dvdnav_open(dvdnav_t** dest, char *path); 

/**
 * Closes a dvdnav_t previously opened with dvdnav_open(), freeing any 
 * memory associated with it.
 *
 * Arguments:
 *   self     -- dvdnav_t to close.
 */
dvdnav_status_t dvdnav_close(dvdnav_t *self);

/**
 * Resets the VM and buffers in a previously opened dvdnav
 *
 * Arguments:
 *   self     -- dvdnav_t to reset.
 */
dvdnav_status_t dvdnav_reset(dvdnav_t *self);

/**
 * Fills a pointer wiht a value pointing to a string describing
 * the path associated with an open dvdnav_t. It assigns it NULL
 * on error.
 *
 * Arguments:
 *   path     -- Pointer to char* to fill in.
 */
dvdnav_status_t dvdnav_path(dvdnav_t *self, char** path);

/**
 * Returns a human-readable string describing the last error.
 *
 * Returns: A pointer to said string.
 */
char* dvdnav_err_to_string(dvdnav_t *self);

/** DVD Player characteristics (FIXME: Need more entries) **/

/**
 * Returns the region mask (bit 0 - region 1, bit 1 - R 2, etc) of the
 * VM
 */
dvdnav_status_t dvdnav_get_region_mask(dvdnav_t *self, int *region);

/**
 * Sets the region mask of the VM.
 *
 * mask: 0x00..0xff
 */
dvdnav_status_t dvdnav_set_region_mask(dvdnav_t *self, int mask);

/**
 * Specify whether read-ahead caching should be used 
 * 
 * use_readahead: 0 - no, 1 - yes
 */
dvdnav_status_t dvdnav_set_readahead_flag(dvdnav_t *self, int use_readahead);

/**
 * Query state of readahead flag
 *
 * flag: pointer to int to recieve flag value
 */
dvdnav_status_t dvdnav_get_readahead_flag(dvdnav_t *self, int* flag);

/** Getting data **/

/**
 * Gets the next block off the DVD places it in 'buf'. If there is any
 * special information, the value pointed to by 'event' gets set 
 * accordingly.
 *
 * If 'event' is DVDNAV_BLOCK_OK then 'buf' is filled with the next block
 * (note that means it has to be at /least/ 2048 bytes big). 'len' is
 * then set to 2048.
 *
 * Otherwise, buf is filled with an appropriate event structure and
 * len is set to the length of that structure,
 */
dvdnav_status_t dvdnav_get_next_block(dvdnav_t *self, unsigned char *buf,
				      int *event, int *len);

/** Navigation **/

/**
 * Returns the number of titles on the disk in titles.
 */
dvdnav_status_t dvdnav_get_number_of_titles(dvdnav_t *self, int *titles);

/**
 * Returns the number of programs within the current title in programs.
 */
dvdnav_status_t dvdnav_get_number_of_programs(dvdnav_t *self, int *programs);

/**
 * If we are currently in a still-frame, skip it.
 */
dvdnav_status_t dvdnav_still_skip(dvdnav_t *self);

/**
 * Plays a specified title of the DVD 
 *
 * Arguments:
 *   title  -- 1..99
 */
dvdnav_status_t dvdnav_title_play(dvdnav_t *self, int title);

/**
 * Plays the specifiec title, starting from the specified
 * part (chapter)
 *
 * Arguments:
 *   title  -- 1..99
 *   part   -- 1..999
 */
dvdnav_status_t dvdnav_part_play(dvdnav_t *self, int title, int part);

/**
 * Play the specified amount of parts of the specified title of
 * the DVD then STOP.
 *
 * Arguments:
 *   title  -- 1..99
 *   part   -- 1..999
 *   parts_to_play -- 1..999
 */
dvdnav_status_t dvdnav_part_play_auto_stop(dvdnav_t *self, int title,
					  int part, int parts_to_play);

/**
 * Play the specified title starting from the specified time
 *
 * Arguments:
 *   title -- 1..99
 *   time: (timecode) hours, minutes, seconds + frames.
 */
dvdnav_status_t dvdnav_time_play(dvdnav_t *self, int title,
				unsigned long int time);

/**
 * Stops playing the current title (causes a STOP action in
 * dvdnav_get_next_block).
 */
dvdnav_status_t dvdnav_stop(dvdnav_t *self);

/**
 * Stop playing current title and play the "GoUp"-program chain  
 */
dvdnav_status_t dvdnav_go_up(dvdnav_t *self);

/** SEARCHING **/

/**
 * Stop playing the current title and start playback of the title
 * from the specified timecode.
 *
 * Arguments:
 *   time -- timecode.
 */
dvdnav_status_t dvdnav_time_search(dvdnav_t *self, 
				  unsigned long int time);

/**
 * Stop playing the current title and start playback of the title
 * from the specified sector offset.
 *
 * Arguments:
 *   offset -- sector offset.
 *   origin -- Start from here, start or end.
 */
dvdnav_status_t dvdnav_sector_search(dvdnav_t *self, 
				  unsigned long int offset, int origin);

/**
 * Stop playing the current title and start playback of the title
 * from the specified part (chapter).
 *
 * Arguments:
 *   part: 1..999
 */
dvdnav_status_t dvdnav_part_search(dvdnav_t *self, int part);

/**
 * Stop playing the current title and start playback of the title
 * from the previous program (if it exists)
 */
dvdnav_status_t dvdnav_prev_pg_search(dvdnav_t *self);

/**
 * Stop playing the current title and start playback of the title
 * from the first program.
 */
dvdnav_status_t dvdnav_top_pg_search(dvdnav_t *self);

/**
 * Stop playing the current title and start playback of the title
 * from the next program (if it exists)
 */
dvdnav_status_t dvdnav_next_pg_search(dvdnav_t *self);

/**
 * Stop playing the current title and jump to the specified menu.
 *
 * Arguments:
 *   menu -- Which menu to call
 */
dvdnav_status_t dvdnav_menu_call(dvdnav_t *self, DVDMenuID_t menu);

/**
 * Return the title number and chapter currently being played or
 * -1 if in a menu.
 */
dvdnav_status_t dvdnav_current_title_info(dvdnav_t *self, int *title,
			     		  int *part);

/**
 * Return a string describing the title
 */
dvdnav_status_t dvdnav_get_title_string(dvdnav_t *self, char **title_str);

/**
 * Return the current position (in blocks) within the current
 * part and the length (in blocks) of said part.
 */
dvdnav_status_t dvdnav_get_position(dvdnav_t *self, unsigned int* pos,
				    unsigned int *len);

/**
 * Return the current position (in blocks) within the current
 * title and the length (in blocks) of said title.
 */
dvdnav_status_t dvdnav_get_position_in_title(dvdnav_t *self,
					     unsigned int* pos,
					     unsigned int *len);

/** Highlights **/

/**
 * Set the value pointed to to the currently highlighted button 
 * number (1..36) or 0 if no button is highlighed.
 *
 * Arguments:
 *   button -- Pointer to the value to fill in.
 */
dvdnav_status_t dvdnav_get_current_highlight(dvdnav_t *self, int* button);

/**
 * Move button highlight around (e.g. with arrow keys)
 */
dvdnav_status_t dvdnav_upper_button_select(dvdnav_t *self);
dvdnav_status_t dvdnav_lower_button_select(dvdnav_t *self);
dvdnav_status_t dvdnav_right_button_select(dvdnav_t *self);
dvdnav_status_t dvdnav_left_button_select(dvdnav_t *self);

/**
 * Activate (press) highlighted button
 */
dvdnav_status_t dvdnav_button_activate(dvdnav_t *self);

/**
 * Highlight a specific button button
 *
 * button -- 1..39.
 */
dvdnav_status_t dvdnav_button_select(dvdnav_t *self, int button);

/**
 * Activate (press) specified button.
 *
 * Arguments:
 *   button: 1..36
 */
dvdnav_status_t dvdnav_button_select_and_activate(dvdnav_t *self, int button);

/**
 * Select button at specified (image) co-ordinates.
 *
 * Arguments:
 *   x,y: Image co-ordinates
 */
dvdnav_status_t dvdnav_mouse_select(dvdnav_t *self, int x, int y);

/**
 * Activate (press) button at specified co-ordinates.
 */
dvdnav_status_t dvdnav_mouse_activate(dvdnav_t *self, int x, int y);

/** i18n **/

/**
 * Set specified menu language.
 *
 * Arguments:
 *   code: 1 char ISO 639 Language code 
 */
dvdnav_status_t dvdnav_menu_languge_select(dvdnav_t *self,
					  char *code);

/** SPU/Audio streams **/

/**
 * Set a specific PHYSICAL MPEG stream.
 *
 * Arguments:
 *   audio: 0..7
 */
dvdnav_status_t dvdnav_physical_audio_stream_change(dvdnav_t *self,
						   int audio);

/**
 * Set a specific logical audio stream.
 *
 * Arguments:
 *   audio: 0..7
 */
dvdnav_status_t dvdnav_logical_audio_stream_change(dvdnav_t *self,
		       				  int audio);

/**
 * Set the int pointed to to the current PHYSICAL audio
 * stream.
 *
 * Arguments:
 *   audio: Pointer to value
 */
dvdnav_status_t dvdnav_get_physical_audio_stream(dvdnav_t *self, int* audio);

/**
 * Set the int pointed to to the current LOGICAL audio
 * stream.
 *
 * Arguments:
 *   audio: Pointer to value
 */
dvdnav_status_t dvdnav_get_logical_audio_stream(dvdnav_t *self, int* audio);

/**
 * Set a specific PHYSICAL MPEG SPU stream and whether it should be
 * displayed.
 *
 * Arguments:
 *   stream: 0..31 or 63 (dummy)
 *   display: 0..1
 */
dvdnav_status_t dvdnav_physical_spu_stream_change(dvdnav_t *self,
					  	  int stream, int display);

/**
 * Set a specific LOGICAL SPU stream and whether it should be
 * displayed.
 *
 * Arguments:
 *   stream: 0..31 or 63 (dummy)
 *   display: 0..1
 */
dvdnav_status_t dvdnav_logical_spu_stream_change(dvdnav_t *self,
						 int stream, int display);

/**
 * Set the ints pointed to to the current PHYSICAL SPU
 * stream & display flag.
 *
 * Arguments:
 *   stream, display: Pointers to value
 */
dvdnav_status_t dvdnav_get_physical_spu_stream(dvdnav_t *self,
					       int* stream, int* disply);

/**
 * Set the ints pointed to to the current LOGICAL SPU
 * stream & display flag.
 *
 * Arguments:
 *   stream, display: Pointers to value
 */
dvdnav_status_t dvdnav_get_logical_spu_stream(dvdnav_t *self,
		       			     int* stream, int* disply);

/** ANGLES **/

/**
 * Sets the current angle 
 *
 * Arguments:
 *   angle: 1..9
 */
dvdnav_status_t dvdnav_angle_change(dvdnav_t *self, int angle);

/**
 * Returns the current angle and number of angles present
 */
dvdnav_status_t dvdnav_get_angle_info(dvdnav_t *self, int* current_angle,
				     int *number_of_angles);

/**
 * Converts a *logical* subpicture stream id into country code 
 * (returns 0xffff if no such stream).
 */
uint16_t dvdnav_audio_stream_to_lang(dvdnav_t *self, uint8_t stream);
uint16_t dvdnav_spu_stream_to_lang(dvdnav_t *self, uint8_t stream);

/**
 * Get substream id for logical stream *_num.
 */
int8_t dvdnav_get_audio_logical_stream(dvdnav_t *self, uint8_t audio_num);
int8_t dvdnav_get_spu_logical_stream(dvdnav_t *self, uint8_t subp_num);

/**
 * Get active spu stream.
 */
int8_t dvdnav_get_active_spu_stream(dvdnav_t *self);

/**
 * Get video aspect
 */
uint8_t dvdnav_get_video_aspect(dvdnav_t *self);

/* Following functions returns:
 *  -1 on failure,
 *   1 if condition is true,
 *   0 if condition is false
 */
/***  Current VM domain state  ***/
/* First Play domain. (Menu) */
int8_t dvdnav_is_domain_fp(dvdnav_t *self);
/* Video management Menu domain. (Menu) */
int8_t dvdnav_is_domain_vmgm(dvdnav_t *self);
/* Video Title Menu domain (Menu) */
int8_t dvdnav_is_domain_vtsm(dvdnav_t *self);
/* Video Title domain (playing movie). */
int8_t dvdnav_is_domain_vts(dvdnav_t *self);

#ifdef __cplusplus
}
#endif

#endif /* DVDNAV_H_INCLUDED */
