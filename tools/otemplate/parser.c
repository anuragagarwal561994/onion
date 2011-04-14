/*
	Onion HTTP server library
	Copyright (C) 2010-2011 David Moreno Montero

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	*/

#include <stdio.h>

#include "onion/log.h"

#include "parser.h"
#include "tags.h"
#include "variables.h"

/// Set current parser status
static void set_mode(parser_status *status, int mode);

/**
 * @short Main parsing loop.
 */
void parse_template(parser_status *status){
	int c;
	while ( (status->c=fgetc(status->in)) >= 0){
		c=status->c;
		switch(status->mode){
			case TEXT:
				if (c=='{')
					set_mode(status, BEGIN);
				else
					add_char(status, c);
				break;
			case BEGIN:
				if (c=='{')
					set_mode(status, VARIABLE);
				else if (c=='%')
					set_mode(status, TAG);
				else{
					set_mode(status, TEXT);
					add_char(status, '{');
					add_char(status, c);
				}
				break;
			case VARIABLE:
				if (c=='}')
					set_mode(status, END_VARIABLE);
				else
					add_char(status, c);
				break;
			case TAG:
				if (c=='%')
					set_mode(status, END_TAG);
				else
					add_char(status, c);
				break;
			case END_VARIABLE:
				if (c=='}')
					set_mode(status, TEXT);
				else{
					set_mode(status, VARIABLE);
					add_char(status, '}');
					add_char(status, c);
				}
				break;
			case END_TAG:
				if (c=='}')
					set_mode(status, TEXT);
				else{
					set_mode(status, END_TAG);
					add_char(status, '%');
					add_char(status, c);
				}
				break;
			default:
				ONION_ERROR("Unknown mode %d",status->mode);
				status->status=1;
				return;
		}
	}
	set_mode(status, END);
}

/**
 * @short One block read from in, prepare the output.
 * 
 * Depending on the mode of the block it calls the appropiate handler: variable, tag or just write text.
 */
void write_block(parser_status *st, block *b){
	int mode=st->last_wmode;
	block_add_char(b, '\0');
	b->pos--;
	ONION_DEBUG("Write mode %d, code %s", mode, b->data);
	switch(mode){
		case TEXT:
			if (b->pos){
				int oldl=b->pos;
				block_safe_for_printf(b);
				function_add_code(st, "  onion_response_write(res, \"%s\", %d);\n", b->data, oldl);
			}
			break;
		case VARIABLE:
			write_variable(st, b);
			break;
		case TAG:
			write_tag(st, b);
			break;
		default:
			ONION_ERROR("Unknown final mode %d", mode);
	}
	st->rawblock->pos=0;
}

/// Read a char from the st->in-
void add_char(parser_status *st, char c){
	block_add_char(st->rawblock, c);
}

/// Mode change, depending of the mode, may indicate that a block must be written
void set_mode(parser_status *status, int mode){
	if (mode<16){
		write_block(status, status->rawblock);
		status->last_wmode=mode;
	}
	status->mode=mode;
}
