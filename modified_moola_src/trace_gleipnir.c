//
//  trace_gleipnir.c  (trace file input, gleipnir formats for Moola Multicore Cache Simulator)
//  Copyright (c) 2013 Charles Shelor.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  Contact charles.shelor@gmail.com  or  Krishna.Kavi@unt.edu
//  Net-Centric Software and Systems I/UCRC.  http://netcentric.unt.edu/content/welcome
//
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

#include "moola.h"


//	"trace_gleipnir.c" incorporates versions of the trace file access functions for the multicore
//	memory trace files with Gleipnir source code tracking information and from text or gzip compressed files.

//  The close, open, and reopen functions are the same for data and non-data versions.
//	The different versions of the trace access functions are grouped together.  The moola
//	main program assigns the appropriate trace access functions to the moola function pointers
//	based on the input format specification passed to moola at run time.


/* Gleipnir format.
 * First line:  START PID <number>
 *			<number> is the process ID generating the trace file
 * Format:
 * L/S/M | VADDRESS | PADDRESS | SIZE | TID | SEG | FUNCTION | SCOPE | ELEMENT
 * X | ADDRESS | MALLOC | SIZE | NAME
 * L/S/M | ADDRESS | SIZE | TID | FUNCTION | H-enum | ELEMENT
 */

/* NOTE
 * Each structure will be stripped (will not track individual elements)
 * Same is true for arrays.
 *
 * Will not track Global vs Local variable type at this point
 */

//  TBD  update the following comment block as appropriate for gleipnir as shown above
//
//	The trace record format is defined as the following:
//	type proc_id address [ size [ data [ segment ] ] ]
//		type		identifies the record type and is {A, F, I, L, M, P, S, #}
//		proc_id		decimal 0-31 processor identification
//		address		hexadecimal address of transaction
//		size		decimal size of transaction for A, I, L, M, S records
//		data		hexadecimal data for I, L, M, S records
//		segment		character segment code G, H, I, S for L, M, S records
//
//	type of '#' is a comment record, remainder of record is ignored
//	blanks are required as field separators, but extra blanks are ignored
//	blanks between data bytes in the data field are ignored (but not required)
//
//	A – memory allocation record (pid, adrs, size)
//	F – memory free record (pid, adrs)
//	I – instruction fetch record (pid, adrs, size, data)
//	L – load memory record (pid, adrs, size, data, segment)
//	M – modify memory record (pid, adrs, size, data, segment)
//	P – stack pointer change record (pid, adrs)
//	S – store memory record (pid, adrs, size, data, segment)
//	# - trace comment record (record is ignored)



//	The following variables are either static or global to functions in this file
//  The global data is required for the in-line functions and source macros to operate properly
//	The static data is required for maintaining state information between calls to the functions
uint8_t		*bytes;			//	pointer to data value bytes (function global)
int16_t		bndx;			//	index to the current byte of data (function global)
char		c;				//  character from input trace file (function global)
char		*cptr;			//	pointer to next character in ibfr (function global)
int16_t		crntfndx;		//	current file index for use by sub function error reporting (function global)
gzFile		*gzfil[8];		//	pointers to gzipped trace files for input (static)
char		ibfr[600];		//	buffer for input, 160 should be long enough for each line (function global)
int64_t		in_lineno[8];	//	line number of most recently read line in the files (static)
FILE		*txtfil[8];		//	pointers to text trace files for input (static)
int64_t		val;			//	temporary generation of an input value (function global)

//  These macros and in-line functions improve code readability by abstracting these common actions
#define FIND_WS 	while (c != ' ' && c != '\t' && c != '\n') { c = *cptr++; }
#define SKIP_WS 	while (c == ' ' || c == '\t') { c = *cptr++; }

void inline glget_dec(void);		//  get decimal value from input trace line
void inline glget_hex(void);		//  get hexadecimal value from input trace line
void inline glget_hexbytes(void);	//  get hexadecimal value from input trace line as sequence of bytes
char *process_nodata(memref *mr);	//  process trace record


//  These lookup tables allow fast identification of character types and values
static char char_val[256] = { 0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  /* 00-0f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 10-1f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 20-2f */
	0,  1,  2,  3,    4,  5,  6,  7,    8,  9,  0,  0,    0,  0,  0,  0,  /* 30-3f */
	0, 10, 11, 12,   13, 14, 15,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 40-4f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 50-5f */
	0, 10, 11, 12,   13, 14, 15,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 60-6f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 70-7f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 80-8f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 90-9f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* a0-af */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* b0-bf */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* c0-cf */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* d0-df */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* e0-ef */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0}; /* f0-ff */

static char ishex[256] = { 0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  /* 00-0f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 10-1f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 20-2f */
	1,  1,  1,  1,    1,  1,  1,  1,    1,  1,  0,  0,    0,  0,  0,  0,  /* 30-3f */
	0,  1,  1,  1,    1,  1,  1,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 40-4f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 50-5f */
	0,  1,  1,  1,    1,  1,  1,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 60-6f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 70-7f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 80-8f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 90-9f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* a0-af */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* b0-bf */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* c0-cf */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* d0-df */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* e0-ef */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0}; /* f0-ff */

static char isdec[256] = { 0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,  /* 00-0f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 10-1f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 20-2f */
	1,  1,  1,  1,    1,  1,  1,  1,    1,  1,  0,  0,    0,  0,  0,  0,  /* 30-3f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 40-4f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 50-5f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 60-6f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 70-7f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 80-8f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* 90-9f */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* a0-af */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* b0-bf */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* c0-cf */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* d0-df */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,  /* e0-ef */
	0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0,    0,  0,  0,  0}; /* f0-ff */



//  function to close the gzipped trace file (both with or without data values)
//	'fndx' is the index into the global gzipped-file list of the file to close
//	The entry in the list of file pointers is set to NULL
void trace_close_gleipnir_gz(int16_t fndx) {
	gzclose(gzfil[fndx]);
	gzfil[fndx] = NULL;
	return;
}



//  function to close the text trace file (both with or without data values)
//	'fndx' is the index into the global text-file list of the file to close
//	The entry in the list of file pointers is set to NULL
void trace_close_gleipnir_txt(int16_t fndx) {
	close(txtfil[fndx]);
	txtfil[fndx] = NULL;
	return;
}



//  function to open the gzipped trace file
//	'fndx' is the index into the global 'in_fnames' of file names
//	The global list of gzipped file pointers is updated with the pointer to the open file
//	returns 0 for error, 1 for success
int32_t trace_open_gleipnir_gz(int16_t fndx) {
	
	gzfil[fndx] = gzopen(in_fnames[fndx], "r");
	if (gzfil[fndx] == NULL) {
		printf("Error opening file '%s' for trace input.\n", in_fnames[fndx]);
		return 0;
	} else {
		in_lineno[fndx] = 0;
		return 1;
	}
	
}


//  function to open the text trace file
//	'fndx' is the index into the global 'in_fnames' of file names
//	The global list of gzipped file pointers is updated with the pointer to the open file
//	returns 0 for error, 1 for success
int32_t trace_open_gleipnir_txt(int16_t fndx) {
	
	txtfil[fndx] = fopen(in_fnames[fndx], "r");
	if (txtfil[fndx] == NULL) {
		printf("Error opening file '%s' for trace input.\n", in_fnames[fndx]);
		return 0;
	} else {
		in_lineno[fndx] = 0;
		return 1;
	}
	
}



//  function to reopen the gzipped trace file  (reopen does not initialize 'in_lineno')
//	'fndx' is the index into the global 'in_fnames' of file names
//	The global list of gzipped file pointers is updated with the pointer to the open file
//	returns 0 for error, 1 for success
int32_t trace_reopen_gleipnir_gz(int16_t fndx) {
	
	gzfil[fndx] = gzopen(in_fnames[fndx], "r");
	if (gzfil[fndx] == NULL) {
		printf("Error opening file '%s' for trace input.\n", in_fnames[fndx]);
		return 0;
	} else {
		return 1;
	}
	
}


//  function to reopen the text trace file  (reopen does not initialize 'in_lineno')
//	'fndx' is the index into the global 'in_fnames' of file names
//	The global list of gzipped file pointers is updated with the pointer to the open file
//	returns 0 for error, 1 for success
int32_t trace_reopen_gleipnir_txt(int16_t fndx) {
	
	txtfil[fndx] = fopen(in_fnames[fndx], "r");
	if (txtfil[fndx] == NULL) {
		printf("Error opening file '%s' for trace input.\n", in_fnames[fndx]);
		return 0;
	} else {
		return 1;
	}
	
}



//  The trace_read_gleipnir_gz. function reads the next valid trace record from the gzipped,
//	gleipnir format trace file WITHOUT data values.
//  The input buffer is then sent to 'process_values' to decode and place the
//	information into the *mr record that was provided as an input.
//  0 is returned if the end of file was reached without completing a trace record
//	blank lines, comment lines, and invalid trace record lines are skipped

int32_t trace_read_gleipnir_gz(int16_t fndx, memref *mr) {
	//	see list of static/global variables as well as these local variables
	char		*stat;				//	status response for gzgets and process_values
	crntfndx = fndx;				//	set for error messages to use
	while (1) {						//	loop until valid record or end-of-file
		stat = gzgets(gzfil[fndx], ibfr, 160);		//	get next line of input from gzip input file
		if (stat == NULL) {
			//	end of file reached
			return 0;
		}
		in_lineno[fndx]++;			//	increment line number
		stat = process_nodata(mr);
		if (stat) {					//	stat != 0 => valid input, otherwise get new line
			mr->linenmbr = in_lineno[fndx];
			return 1;
		}
	}
}



//  The trace_read_gleipnir_txt  function reads the next valid trace record from the text,
//	gleipnir format trace file WITHOUT data values.
//  The input buffer is then sent to 'process_values' to decode and place the
//	information into the *mr record that was provided as an input.
//  0 is returned if the end of file was reached without completing a trace record
//	blank lines, comment lines, and invalid trace record lines are skipped

int32_t trace_read_gleipnir_txt(int16_t fndx, memref *mr) {
	//	see list of static/global variables as well as these local variables
	char		*stat;				//	status response for fgets and process_values
	crntfndx = fndx;				//	set for error messages to use
	while (1) {						//	loop until valid record or end-of-file
		stat = fgets(ibfr, 600, txtfil[fndx]);		//	get next line of input from text input file
		if (stat == NULL) {
			//	end of file reached
			return 0;
		}
		in_lineno[fndx]++;			//	increment line number
		stat = process_nodata(mr);
		if (stat) {					//	stat != 0 => valid input, otherwise get new line
			mr->linenmbr = in_lineno[fndx];
			return 1;
		}
	}
}



char *process_nodata(memref *mr) {
	int16_t	i;					//	index for copying function and variable names
	
	mr->fname[0] = '\0';		//	some initializations
	mr->scope[0] = '\0';
	mr->vname[0] = '\0';
	
	cptr = ibfr;				//	set to first character in trace record
	c = *cptr++;				//	get 1st char and point to next character
	SKIP_WS						//	white space allowed at start of line (not expected though)
	
	//	get the trace record type code
	switch (c) {
		case 'A':
		case 'a':	mr->oper = XALLOC; break;
		case 'F':
		case 'f':	mr->oper = XFREE; break;
		case 'I':
		case 'i':	mr->oper = MRINSTR; break;
		case 'L':
		case 'l':	mr->oper = MRREAD; break;
		case 'M':
		case 'm':	mr->oper = MRMODFY; break;
		case 'S':
		case 's':	mr->oper = MRWRITE; break;
		case 'P':
		case 'p':	mr->oper = XSTACK; break;
		case 'x':
		case 'X':	return NULL;	//	Gleipnir processing ignores X records
		case '#':	return NULL;	//	skip comment lines
		case '\n':	return NULL;	//	skip blank lines
			
		default:
			fprintf(stderr,
					"Unexpected trace record type, '%c', at column %ld in file %s line %lld\n%s",
					c, cptr - ibfr, in_fnames[crntfndx], in_lineno[crntfndx], ibfr);
			return NULL;
	}
	
	c = *cptr++;				//	"consume" this character and go to next
	if (c == 'T') {
		return NULL;			//	skip the "START PID" record
	}
	if (c != ' '  &&  c != '\t') {
		fprintf(stderr, "Space required after record type at column %ld in file %s, line %lld\n%s",
				cptr - ibfr, in_fnames[crntfndx], in_lineno[crntfndx], ibfr);
		return NULL;			//	require white space after record type code
	}
	
	
	
	//  next field is virtual address in hex
	glget_hex();
	if (!cptr)	return NULL;	//	skip line if error in glget_hex
	mr->virt_adrs = val;		//	save value as the virtual address
	
	//  next field is physical address in hex
	glget_hex();
	if (!cptr)	return NULL;	//	skip line if error in glget_hex
	mr->adrs = val;				//	save value as the physical address
	
	//	next field is transaction size in decimal
	glget_dec();
	if (!cptr)	return NULL;	//	skip line if error in glget_dec
	mr->size = (int32_t) val;	//  save value as the size
	
	//	next field is thread ID in decimal
	glget_dec();
	if (!cptr)	return NULL;	//	skip line if error in glget_dec
	mr->pid = (int8_t) (val % nmbr_cores);		//	save TID % #cores as the processor ID
	
	//	next field is single character indicating Global, Heap, Stack, or 'other'
	SKIP_WS						//	get character code of memory segment and make it 0-4
								//	memory segment 0-4: global, heap, instruction, stack, other
	
	if (mr->oper == MRINSTR) {
		mr->segmnt = INSTR;		//	instruction records have no segment at this time
	} else {
		if (c == 'H' || c == 'h') {
			mr->segmnt = HEAP;
		} else if (c == 'S' || c == 's') {
			mr->segmnt = STACK;
		} else if (c == 'G' || c == 'g') {
			mr->segmnt = GLOBAL;
		} else {
			mr->segmnt = OTHER;
		}
		c = *cptr++;				//	"consume" this character and go to next
	}
	
	//  set up default function/variable/scope values for early EOL return
	strcpy(mr->fname, "_unknown_");
	mr->scope[0] = 'N';
	mr->scope[1] = 'A';
	mr->scope[2] = '\0';
	mr->h_enum = 0;
	switch(mr->segmnt){
		case GLOBAL:
			strcpy(mr->vname, "_global_");
			break;
		case HEAP:
			strcpy(mr->vname, "_heap_");
			break;
		case INSTR:
			strcpy(mr->vname, "_instruction_");
			break;
		case STACK:
			strcpy(mr->vname, "_stack_");
			break;
		default:
			strcpy(mr->vname, "_unknown_");
	}
	
	//  next field should be function name
	SKIP_WS
	if (c == '\n') {
		//  hit end of line, so return the available information
		return 1;				//	return as valid trace record
	}
	
	//	copy function name
	i = 0;
	while (c != '\n'  &&  c != ' '  &&  c != '\t'  &&  i < FSIZE-1) {
		mr->fname[i++] = c;
		c = *cptr++;
	}
	mr->fname[i] = '\0';		//	terminate the string
	if (i >= FSIZE-1) {			//	skip remainder of function name
		while (c != '\n'  &&  c != ' '  &&  c != '\t') c = cptr++;
	}
	
	//  next field should be scope
	SKIP_WS
	if (c == '\n') {
		//  hit end of line, so return the available information
		return 1;				//	return as valid trace record
	}
	mr->scope[0] = c;
	mr->scope[1] = *cptr++;
	
	if (mr->scope[0] == 'H') {
		mr->h_enum = (int32_t) strtol(cptr, NULL, 0);
		mr->scope[1] = '\0';	//	overwrite '-'
	}
	c = *cptr++;				//	consume 2nd scope char and get next
	
	//  next field should be variable
	SKIP_WS
	if (c == '\n') {
		//  hit end of line, so return the available information
		return 1;				//	return as valid trace record
	}
	
	//  copy first component of variable name
	i = 0;
	while (c != '\n'  &&  c != ' '  &&  c != '\t'  &&  c != '['  && c != '.'  &&  i < VSIZE-1) {
		mr->vname[i++] = c;
		c = *cptr++;
	}
	mr->vname[i] = '\0';		//	terminate the string
	return 1;					//	return as valid trace record, skipping remainder (if any) of line
}




//  Various support functions used in the process_nodata function:

void glget_dec(void) {		//  get decimal value from input trace line
	SKIP_WS
	val = 0;
	if (!isdec[c]) {
		fprintf(stderr, "ERROR:  expecting decimal digit at character %ld, file %s, line %lld found '%c'\n%s",
				cptr-ibfr, in_fnames[crntfndx], in_lineno[crntfndx], c, ibfr);
		cptr = NULL;		//  flag error
	} else {
		while (isdec[c]) {		//	first non-decimal character terminates the value
			val = val * 10 + char_val[c];
			c = *cptr++;
		}
	}
	return;
}




void glget_hex(void) {		//  get hexadecimal value from input trace line
	SKIP_WS
	val = 0;
	if (!ishex[c]) {
		fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
				cptr-ibfr, in_fnames[crntfndx], in_lineno[crntfndx], c, ibfr);
		cptr = NULL;		//  flag error
	} else {
		while (ishex[c]) {		//	first non-hex character terminates the value
			val = val * 16 + char_val[c];
			c = *cptr++;
		}
	}
	return;
}


void glget_hexbytes(void) {	//  get hexadecimal value from input trace line as sequence of bytes
									//  val contains the just read size
	for (bndx = 0; bndx < val; bndx++) {
		bytes[bndx] = 0;
		SKIP_WS
		if (!ishex[c]) {
			fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
					cptr-ibfr, in_fnames[crntfndx], in_lineno[crntfndx], c, ibfr);
			cptr = NULL;		//  flag error
			break;
		} else {
			bytes[bndx] = char_val[c];
			c = *cptr++;
			if (!ishex[c]) {
				fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
						cptr-ibfr, in_fnames[crntfndx], in_lineno[crntfndx], c, ibfr);
				cptr = NULL;		//  flag error
				break;
			} else {
				bytes[bndx] = bytes[bndx] * 16 + char_val[c];
				c = *cptr++;
			}
		}
	}
	return;
}
