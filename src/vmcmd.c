/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
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
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#ifdef _MSC_VER
#include <timer.h> /* struct timeval */
#endif

#include "vmcmd.h"


/*  freebsd compatibility */
#ifndef PRIu8
#define PRIu8 "d"
#endif

/*  freebsd compatibility */
#ifndef PRIu16
#define PRIu16 "d"
#endif

static const char *cmp_op_table[] = {
  NULL, "&", "==", "!=", ">=", ">", "<=", "<"
};
static const char *set_op_table[] = {
  NULL, "=", "<->", "+=", "-=", "*=", "/=", "%=", "rnd", "&=", "|=", "^="
};

static const char *link_table[] = {
  "LinkNoLink",  "LinkTopC",    "LinkNextC",   "LinkPrevC",
  NULL,          "LinkTopPG",   "LinkNextPG",  "LinkPrevPG",
  NULL,          "LinkTopPGC",  "LinkNextPGC", "LinkPrevPGC",
  "LinkGoUpPGC", "LinkTailPGC", NULL,          NULL,
  "RSM"
};

static const char *system_reg_table[] = {
  "Menu Description Language Code",
  "Audio Stream Number",
  "Sub-picture Stream Number",
  "Angle Number",
  "Title Track Number",
  "VTS Title Track Number",
  "VTS PGC Number",
  "PTT Number for One_Sequential_PGC_Title",
  "Highlighted Button Number",
  "Navigation Timer",
  "Title PGC Number for Navigation Timer",
  "Audio Mixing Mode for Karaoke",
  "Country Code for Parental Management",
  "Parental Level",
  "Player Configurations for Video",
  "Player Configurations for Audio",
  "Initial Language Code for Audio",
  "Initial Language Code Extension for Audio",
  "Initial Language Code for Sub-picture",
  "Initial Language Code Extension for Sub-picture",
  "Player Regional Code",
  "Reserved 21",
  "Reserved 22",
  "Reserved 23"
};

static const char *system_reg_abbr_table[] = {
  NULL,
  "ASTN",
  "SPSTN",
  "AGLN",
  "TTN",
  "VTS_TTN",
  "TT_PGCN",
  "PTTN",
  "HL_BTNN",
  "NVTMR",
  "NV_PGCN",
  NULL,
  "CC_PLT",
  "PLT",
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

typedef struct {
  uint16_t SPRM[24];
  uint16_t GPRM[16];
  uint8_t  GPRM_mode[16];  /* Need to have some thing to indicate normal/counter mode for every
GPRM */
  struct timeval GPRM_time[16]; /* For counter mode */
} registers_t;

typedef struct
{
  uint64_t instruction;
  uint64_t examined;
  registers_t *registers;
} command_t;

uint32_t vm_getbits(command_t *command, int start, int count) {
  uint64_t result = 0;
  uint64_t bit_mask=~0;  /* I could put -1 instead */
  uint64_t examining = 0;
  int32_t  bits;
  if (count == 0) return 0;

  if ( ((start - count) < -1) ||
       (count > 32) ||
       (start > 63) ||
       (count < 0) ||
       (start < 0) ){
    fprintf(stderr, "Bad call to vm_getbits. Parameter out of range\n");
    assert(0);
  }
  bit_mask >>= 63 - start;
  bits = start + 1 - count;
  examining = ((bit_mask >> bits) << bits );
  command->examined |= examining;
  result = (command->instruction & bit_mask) >> bits;
  return (uint32_t) result;
}
    
static void print_system_reg(uint16_t reg) {
  if(reg < sizeof(system_reg_abbr_table) / sizeof(char *))
    fprintf(MSG_OUT, "%s (SRPM:%d)", system_reg_table[reg], reg);
  else
    fprintf(MSG_OUT, " WARNING: Unknown system register ( reg=%d ) ", reg);
}

static void print_g_reg(uint8_t reg) {
    if(reg < 16)
      fprintf(MSG_OUT, "g[%" PRIu8 "]", reg);
    else
      fprintf(MSG_OUT, " WARNING: Unknown general register ");
}

static void print_reg(uint8_t reg) {
  if(reg & 0x80)
    print_system_reg(reg & 0x7f);
  else
    print_g_reg(reg & 0x7f);
}

static void print_cmp_op(uint8_t op) {
  if(op < sizeof(cmp_op_table) / sizeof(char *) && cmp_op_table[op] != NULL)
    fprintf(MSG_OUT, " %s ", cmp_op_table[op]);
  else
    fprintf(MSG_OUT, " WARNING: Unknown compare op ");
}

static void print_set_op(uint8_t op) {
  if(op < sizeof(set_op_table) / sizeof(char *) && set_op_table[op] != NULL)
    fprintf(MSG_OUT, " %s ", set_op_table[op]);
  else
    fprintf(MSG_OUT, " WARNING: Unknown set op ");
}

static void print_reg_or_data(command_t* command, int immediate, int start) {
  if(immediate) {
    uint32_t i = vm_getbits(command, start, 16);
    
    fprintf(MSG_OUT, "0x%x", i);
    if(isprint(i & 0xff) && isprint((i>>8) & 0xff))
      fprintf(MSG_OUT, " (\"%c%c\")", (char)((i>>8) & 0xff), (char)(i & 0xff));
  } else {
    print_reg(vm_getbits(command, start - 8, 8));
  }
}

static void print_reg_or_data_2(command_t* command, int immediate, int start) {
  if(immediate)
    fprintf(MSG_OUT, "0x%x", vm_getbits(command, start - 1, 7));
  else
    fprintf(MSG_OUT, "g[%" PRIu8 "]", vm_getbits(command, start - 4, 4));
}

static void print_reg_or_data_3(command_t* command, int immediate, int start) {
  if(immediate) {
    uint32_t i = vm_getbits(command, start, 16);
    
    fprintf(MSG_OUT, "0x%x", i);
    if(isprint(i & 0xff) && isprint((i>>8) & 0xff))
      fprintf(MSG_OUT, " (\"%c%c\")", (char)((i>>8) & 0xff), (char)(i & 0xff));
  } else {
    print_reg(vm_getbits(command, start, 8));
  }
}


static void print_if_version_1(command_t* command) {
  uint8_t op = vm_getbits(command, 54, 3);
  
  if(op) {
    fprintf(MSG_OUT, "if (");
    print_g_reg(vm_getbits(command,39,8));
    print_cmp_op(op);
    print_reg_or_data(command, vm_getbits(command, 55,1), 31);
    fprintf(MSG_OUT, ") ");
  }
}

static void print_if_version_2(command_t* command) {
  uint8_t op = vm_getbits(command, 54, 3);
  
  if(op) {
    fprintf(MSG_OUT, "if (");
    print_reg(vm_getbits(command, 15, 8));
    print_cmp_op(op);
    print_reg(vm_getbits(command, 7, 8));
    fprintf(MSG_OUT, ") ");
  }
}

static void print_if_version_3(command_t* command) {
  uint8_t op = vm_getbits(command, 54, 3);
  
  if(op) {
    fprintf(MSG_OUT, "if (");
    print_g_reg(vm_getbits(command, 43, 4));
    print_cmp_op(op);
    print_reg_or_data(command, vm_getbits(command, 55, 1), 15);
    fprintf(MSG_OUT, ") ");
  }
}

static void print_if_version_4(command_t* command) {
  uint8_t op = vm_getbits(command, 54, 3);

  if(op) {
    fprintf(MSG_OUT, "if (");
    print_g_reg(vm_getbits(command, 51, 4));
    print_cmp_op(op);
    print_reg_or_data(command, vm_getbits(command, 55, 1), 31);
    fprintf(MSG_OUT, ") ");
  }
}

static void print_if_version_5(command_t* command) {
  uint8_t op = vm_getbits(command, 54, 3);
  int set_immediate = vm_getbits(command, 60, 1);
  
  if(op) {
    if (set_immediate) {
      fprintf(MSG_OUT, "if (");
      print_g_reg(vm_getbits(command, 31, 8));
      print_cmp_op(op);
      print_reg(vm_getbits(command, 23, 8));
      fprintf(MSG_OUT, ") ");
    } else {
      fprintf(MSG_OUT, "if (");
      print_g_reg(vm_getbits(command, 39, 8));
      print_cmp_op(op);
      print_reg_or_data(command, vm_getbits(command, 55, 1), 31);
      fprintf(MSG_OUT, ") ");
    }
  }
}

static void print_special_instruction(command_t* command) {
  uint8_t op = vm_getbits(command, 51, 4);
  
  switch(op) {
    case 0: /*  NOP */
      fprintf(MSG_OUT, "Nop");
      break;
    case 1: /*  Goto line */
      fprintf(MSG_OUT, "Goto %" PRIu8, vm_getbits(command, 7, 8));
      break;
    case 2: /*  Break */
      fprintf(MSG_OUT, "Break");
      break;
    case 3: /*  Parental level */
      fprintf(MSG_OUT, "SetTmpPML %" PRIu8 ", Goto %" PRIu8, 
	      vm_getbits(command, 11, 4), vm_getbits(command, 7, 8));
      break;
    default:
      fprintf(MSG_OUT, "WARNING: Unknown special instruction (%i)", 
	      vm_getbits(command, 51, 4));
  }
}

static void print_linksub_instruction(command_t* command) {
  uint32_t linkop = vm_getbits(command, 7, 8);
  uint32_t button = vm_getbits(command, 15, 6);
  
  if(linkop < sizeof(link_table)/sizeof(char *) && link_table[linkop] != NULL)
    fprintf(MSG_OUT, "%s (button %" PRIu8 ")", link_table[linkop], button);
  else
    fprintf(MSG_OUT, "WARNING: Unknown linksub instruction (%i)", linkop);
}

static void print_link_instruction(command_t* command, int optional) {
  uint8_t op = vm_getbits(command, 51, 4);
  
  if(optional && op)
    fprintf(MSG_OUT, ", ");
  
  switch(op) {
    case 0:
      if(!optional)
      fprintf(MSG_OUT, "WARNING: NOP (link)!");
      break;
    case 1:
      print_linksub_instruction(command);
      break;
    case 4:
      fprintf(MSG_OUT, "LinkPGCN %" PRIu16, vm_getbits(command, 14, 15));
      break;
    case 5:
      fprintf(MSG_OUT, "LinkPTT %" PRIu16 " (button %" PRIu8 ")", 
	      vm_getbits(command, 9, 10), vm_getbits(command, 15, 6));
      break;
    case 6:
      fprintf(MSG_OUT, "LinkPGN %" PRIu8 " (button %" PRIu8 ")", 
	      vm_getbits(command, 6, 7), vm_getbits(command, 15, 6));
      break;
    case 7:
      fprintf(MSG_OUT, "LinkCN %" PRIu8 " (button %" PRIu8 ")", 
	      vm_getbits(command, 7, 8), vm_getbits(command, 15, 6));
      break;
    default:
      fprintf(MSG_OUT, "WARNING: Unknown link instruction");
  }
}

static void print_jump_instruction(command_t* command) {
  switch(vm_getbits(command, 51, 4)) {
    case 1:
      fprintf(MSG_OUT, "Exit");
      break;
    case 2:
      fprintf(MSG_OUT, "JumpTT %" PRIu8, vm_getbits(command, 22, 7));
      break;
    case 3:
      fprintf(MSG_OUT, "JumpVTS_TT %" PRIu8, vm_getbits(command, 22, 7));
      break;
    case 5:
      fprintf(MSG_OUT, "JumpVTS_PTT %" PRIu8 ":%" PRIu16, 
	      vm_getbits(command, 22, 7), vm_getbits(command, 41, 10));
      break;
    case 6:
      switch(vm_getbits(command, 23, 2)) {
        case 0:
          fprintf(MSG_OUT, "JumpSS FP");
          break;
        case 1:
          fprintf(MSG_OUT, "JumpSS VMGM (menu %" PRIu8 ")", vm_getbits(command, 19, 4));
          break;
        case 2:
          fprintf(MSG_OUT, "JumpSS VTSM (vts %" PRIu8 ", title %" PRIu8 
		  ", menu %" PRIu8 ")", vm_getbits(command, 30, 7), vm_getbits(command, 38, 7), vm_getbits(command, 19, 4));
          break;
        case 3:
          fprintf(MSG_OUT, "JumpSS VMGM (pgc %" PRIu8 ")", vm_getbits(command, 46, 15));
          break;
        }
      break;
    case 8:
      switch(vm_getbits(command, 23, 2)) {
        case 0:
          fprintf(MSG_OUT, "CallSS FP (rsm_cell %" PRIu8 ")",
              vm_getbits(command, 31, 8));
          break;
        case 1:
          fprintf(MSG_OUT, "CallSS VMGM (menu %" PRIu8 
		  ", rsm_cell %" PRIu8 ")", vm_getbits(command, 19, 4), vm_getbits(command, 31, 8));
          break;
        case 2:
          fprintf(MSG_OUT, "CallSS VTSM (menu %" PRIu8 
		  ", rsm_cell %" PRIu8 ")", vm_getbits(command, 19, 4), vm_getbits(command, 31, 8));
          break;
        case 3:
          fprintf(MSG_OUT, "CallSS VMGM (pgc %" PRIu8 ", rsm_cell %" PRIu8 ")", 
		  vm_getbits(command, 46, 15), vm_getbits(command, 31, 8));
          break;
      }
      break;
    default:
      fprintf(MSG_OUT, "WARNING: Unknown Jump/Call instruction");
  }
}

static void print_system_set(command_t* command) {
  int i;
/* FIXME: What about SPRM11 ? Karaoke */
/*        Surely there must be some system set command for that ? */
  
  switch(vm_getbits(command, 59, 4)) {
    case 1: /*  Set system reg 1 &| 2 &| 3 (Audio, Subp. Angle) */
      for(i = 1; i <= 3; i++) {
        if(vm_getbits(command, 47 - (i*8), 1)) {
          print_system_reg(i);
          fprintf(MSG_OUT, " = ");
          print_reg_or_data_2(command, vm_getbits(command, 60, 1), 47 - (i*8) );
          fprintf(MSG_OUT, " ");
        }
      }
      break;
    case 2: /*  Set system reg 9 & 10 (Navigation timer, Title PGC number) */
      print_system_reg(9);
      fprintf(MSG_OUT, " = ");
      print_reg_or_data(command, vm_getbits(command, 60, 1), 47);
      fprintf(MSG_OUT, " ");
      print_system_reg(10);
      fprintf(MSG_OUT, " = %" PRIu16, vm_getbits(command, 30, 15)); /*  ?? */
      break;
    case 3: /*  Mode: Counter / Register + Set */
      fprintf(MSG_OUT, "SetMode ");
      if(vm_getbits(command, 23, 1))
	fprintf(MSG_OUT, "Counter ");
      else
	fprintf(MSG_OUT, "Register ");
      print_g_reg(vm_getbits(command, 19, 4));
      print_set_op(0x1); /*  '=' */
      print_reg_or_data(command, vm_getbits(command, 60, 1), 47);
      break;
    case 6: /*  Set system reg 8 (Highlighted button) */
      print_system_reg(8);
      if(vm_getbits(command, 60, 1)) /*  immediate */
        fprintf(MSG_OUT, " = 0x%x (button no %d)", vm_getbits(command, 31, 16), vm_getbits(command, 31, 6));
      else
        fprintf(MSG_OUT, " = g[%" PRIu8 "]", vm_getbits(command, 19, 4));
      break;
    default:
      fprintf(MSG_OUT, "WARNING: Unknown system set instruction (%i)", 
	      vm_getbits(command, 59, 4));
  }
}

static void print_set_version_1(command_t* command) {
  uint8_t set_op = vm_getbits(command, 59, 4);
  
  if(set_op) {
    print_g_reg(vm_getbits(command, 35, 4));
    print_set_op(set_op);
    print_reg_or_data(command, vm_getbits(command, 60, 1), 31);
  } else {
    fprintf(MSG_OUT, "NOP");
  }
}

static void print_set_version_2(command_t* command) {
  uint8_t set_op = vm_getbits(command, 59, 4);
  
  if(set_op) {
    print_g_reg(vm_getbits(command, 51, 4));
    print_set_op(set_op);
    print_reg_or_data(command, vm_getbits(command, 60, 1), 47);
  } else {
    fprintf(MSG_OUT, "NOP");
  }
}

static void print_set_version_3(command_t* command) {
  uint8_t set_op = vm_getbits(command, 59, 4);
  
  if(set_op) {
    print_g_reg(vm_getbits(command, 51, 4));
    print_set_op(set_op);
    print_reg_or_data_3(command, vm_getbits(command, 60, 1), 47);
  } else {
    fprintf(MSG_OUT, "NOP");
  }
}


void vm_print_mnemonic(vm_cmd_t *vm_command)  {
  command_t command;
  command.instruction =( (uint64_t) vm_command->bytes[0] << 56 ) |
        ( (uint64_t) vm_command->bytes[1] << 48 ) |
        ( (uint64_t) vm_command->bytes[2] << 40 ) |
        ( (uint64_t) vm_command->bytes[3] << 32 ) |
        ( (uint64_t) vm_command->bytes[4] << 24 ) |
        ( (uint64_t) vm_command->bytes[5] << 16 ) |
        ( (uint64_t) vm_command->bytes[6] <<  8 ) |
          (uint64_t) vm_command->bytes[7] ;
  command.examined = 0; 

  switch(vm_getbits(&command,63,3)) { /* three first bits */
    case 0: /*  Special instructions */
      print_if_version_1(&command);
      print_special_instruction(&command);
      break;
    case 1: /*  Jump/Call or Link instructions */
      if(vm_getbits(&command,60,1)) {
        print_if_version_2(&command);
        print_jump_instruction(&command);
      } else {
        print_if_version_1(&command);
        print_link_instruction(&command, 0); /*  must be pressent */
      }
      break;
    case 2: /*  Set System Parameters instructions */
      print_if_version_2(&command);
      print_system_set(&command);
      print_link_instruction(&command, 1); /*  either 'if' or 'link' */
      break;
    case 3: /*  Set General Parameters instructions */
      print_if_version_3(&command);
      print_set_version_1(&command);
      print_link_instruction(&command, 1); /*  either 'if' or 'link' */
      break;
    case 4: /*  Set, Compare -> LinkSub instructions */
      print_set_version_2(&command);
      fprintf(MSG_OUT, ", ");
      print_if_version_4(&command);
      print_linksub_instruction(&command);
      break;
    case 5: /*  Compare -> (Set and LinkSub) instructions */
      print_if_version_5(&command);
      fprintf(MSG_OUT, "{ ");
      print_set_version_3(&command);
      fprintf(MSG_OUT, ", ");
      print_linksub_instruction(&command);
      fprintf(MSG_OUT, " }");
      break;
    case 6: /*  Compare -> Set, always LinkSub instructions */
      print_if_version_5(&command);
      fprintf(MSG_OUT, "{ ");
      print_set_version_3(&command);
      fprintf(MSG_OUT, " } ");
      print_linksub_instruction(&command);
      break;
    default:
      fprintf(MSG_OUT, "WARNING: Unknown instruction type (%i)", vm_getbits(&command, 63, 3));
  }
  /*  Check if there still are bits set that were not examined */
  
  if(command.instruction & ~ command.examined) {
    fprintf(MSG_OUT, " libdvdnav: vmcmd.c: [WARNING, unknown bits:");
    fprintf(MSG_OUT, " %08llx", (command.instruction & ~ command.examined) );
    fprintf(MSG_OUT, "]");
  }
}

void vm_print_cmd(int row, vm_cmd_t *vm_command) {
  int i;

  fprintf(MSG_OUT, "(%03d) ", row + 1);
  for(i = 0; i < 8; i++)
    fprintf(MSG_OUT, "%02x ", vm_command->bytes[i]);
  fprintf(MSG_OUT, "| ");

  vm_print_mnemonic(vm_command);
  fprintf(MSG_OUT, "\n");
}

/*
 * $Log$
 * Revision 1.14  2003/05/05 00:25:11  tchamp
 * Changed the linkage for msvc
 *
 * Revision 1.12  2003/05/01 16:44:43  tchamp
 * ifo_dump should now build with msvc version of libdvdnav
 *
 * Revision 1.11  2003/04/28 15:17:18  jcdutton
 * Update ifodump to work with new libdvdnav cvs, instead of needing libdvdread.
 *
 * Revision 1.10  2003/04/05 16:31:53  jcdutton
 * Some minor changes and updates.
 *
 * Revision 1.9  2003/04/05 15:23:07  jcdutton
 * Minor updates
 *
 * Revision 1.8  2003/04/05 13:03:49  jcdutton
 * Small updates.
 *
 * Revision 1.7  2003/04/05 12:45:47  jcdutton
 * Use MSG_OUT instead of stdout.
 *
 * Revision 1.6  2003/04/05 09:15:43  jcdutton
 * Minor fix
 *
 * Revision 1.5  2003/04/03 11:05:20  jcdutton
 * Tidy up.
 *
 * Revision 1.4  2003/04/03 11:01:36  jcdutton
 * Change byte references to bits.
 *
 * Revision 1.3  2003/04/03 10:51:28  jcdutton
 * Change getbits start param to actually start at the right bit.
 * e.g. lowest bit in a 64 bit number bit 0 and represents value of 1
 * next bit is a 64 bit number is bit 1 and represents a value of 2
 * next bit is a 64 bit number is bit 2 and represents a value of 4
 *
 * Before, a start value of 0 would represent bit 63, and a start value of 63 would represent bit 0.
 *
 * So, I changed it to be more common sense.
 *
 * Revision 1.2  2003/04/02 13:58:19  jcdutton
 * Fix some instruction printouts.
 *
 * Revision 1.1.1.1  2002/08/28 09:48:35  jcdutton
 * Initial import into CVS.
 *
 *
 *
 */
