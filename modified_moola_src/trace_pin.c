//
//  trace_pin.c  (trace file input for PIN multicore format for Moola Multicore Cache Simulator)
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

#include <stdio.h>
#include <stdint.h>
#include "zlib.h"

#include "moola.h"


//	"trace_pin.c" incorporates versions of the trace file access functions for the multicore memory
//	trace files without data values and from text or gzip compressed files from the Intel PIN tools.

//  The close, open, and reopen functions are the same for data and non-data versions.
//	The different versions of the trace access functions are grouped together.  The moola
//	main program assigns the appropriate trace access functions to the moola function pointers
//	based on the input format specification passed to moola at run time.


//  TBD  update the following comment block as appropriate for the 4 function variants
//		and removal of file-name as input to the opens and use of the in_fname[] global data

//  The inputs to tracegz are an index to an open gzipped trace file
//  and a pointer to a memory record to fill with data from the trace file.
//  The function returns 0 if the trace information was valid and filled in.
//  The function returns 1 if an error was encountered and returns 2 if the
//  end of file was reached.
//
//	The trace record format is defined as the following:
//	thr_id type proc_id address size map
//		thr_id		decimal 0-31 thread identification (mapped 1-to-1 to processor)
//		type		identifies the record type and is {I, R, W, #}
//		address		hexadecimal address of transaction (0xHHHHH format)
//		size		decimal size of transaction for I, R, W records
//		map			0 - use address as is and share, 1 - map address for isolation
//
//	type of '#' is a comment record, remainder of record is ignored
//	blanks are required as field separators, but extra blanks are ignored
//	blanks between data bytes in the data field are ignored (but not required)
//
//	I – instruction fetch record (thr_id, type, adrs, size, data)
//	R – read memory record (thr_id, type, adrs, size, data, segment)
//	W – write memory record (thr_id, type, adrs, size, data, segment)
//
//	A – memory allocation record (pid, adrs, size)
//	F – memory free record (pid, adrs)
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
char		c2;				//  second character from input trace file (function global)
char		*cptr;			//	pointer to next character in ibfr (function global)
int16_t		crntfndx;		//	current file index for use by sub function error reporting (function global)
gzFile		*gzfil[8];		//	pointers to gzipped trace files for input (static)
char		ibfr[160];		//	buffer for input, 160 should be long enough for each line (function global)
int64_t		in_lineno[8];	//	line number of most recently read line in the files (static)
int32_t		mapit;			//	from input file, 0 - do not add offset, 1 - add offset to address
FILE		*txtfil[8];		//	pointers to text trace files for input (static)
int64_t		val;			//	temporary generation of an input value (function global)

//  These macros and in-line functions improve code readability by abstracting these common actions
#define FIND_WS 	while (c != ' ' && c != '\t' && c != '\n') { c = *cptr++; }
#define SKIP_WS 	while (c == ' ' || c == '\t') { c = *cptr++; }

void inline get_dec_pin(void);		//  get decimal value from input trace line
void inline get_hex_pin(void);		//  get hexadecimal value from input trace line
void inline get_hexbytes_pin(void);	//  get hexadecimal value from input trace line as sequence of bytes
char *process_pin_data(memref *mr);	//  process trace records containing memory data values
char *process_pin_nodata(memref *mr);	//  process trace records that contain no memory data values


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



//  function to close the gzipped trace file
//	'fndx' is the index into the global gzipped-file list for the file to close
//	The entry in the list of file pointers is set to NULL
void trace_close_pin_gz(int16_t fndx) {
	gzclose(gzfil[fndx]);
	gzfil[fndx] = NULL;
	return;
}



//  function to close the text trace file
//	'fndx' is the index into the global text-file list for the file to close
//	The entry in the list of file pointers is set to NULL
void trace_close_pin_txt(int16_t fndx) {
	close(txtfil[fndx]);
	txtfil[fndx] = NULL;
	return;
}



//  function to open the gzipped trace file
//	'fndx' is the index into the global 'in_fnames' of file names
//	The global list of gzipped file pointers is updated with the pointer to the open file
//	returns 0 for error, 1 for success
int32_t trace_open_pin_gz(int16_t fndx) {
	
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
int32_t trace_open_pin_txt(int16_t fndx) {
	
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
int32_t trace_reopen_pin_gz(int16_t fndx) {
	
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
int32_t trace_reopen_pin_txt(int16_t fndx) {
	
	txtfil[fndx] = fopen(in_fnames[fndx], "r");
	if (txtfil[fndx] == NULL) {
		printf("Error opening file '%s' for trace input.\n", in_fnames[fndx]);
		return 0;
	} else {
		return 1;
	}
	
}



//  The trace_read_pin_gz function reads the next valid trace record from the gzipped,
//	moola format trace file WITHOUT data values.
//  The input buffer is then sent to 'process_moola_nodata' to decode and place the
//	information into the *mr record that was provided as an input.
//  0 is returned if the end of file was reached without completing a trace record
//	blank lines, comment lines, and invalid trace record lines are skipped

int32_t trace_read_pin_gz(int16_t fndx, memref *mr) {
	//	see list of static/global variables as well as these local variables
	char		*stat;				//	status response for gzgets and process_moola_nodata
	crntfndx = fndx;				//	set for error messages to use
	while (1) {						//	loop until valid record or end-of-file
		stat = gzgets(gzfil[fndx], ibfr, 160);		//	get next line of input from gzip input file
		if (stat == NULL) {
			//	end of file reached
			return 0;
		}
		in_lineno[fndx]++;			//	increment line number
		stat = process_pin_nodata(mr);
		if (stat) {					//	stat != 0 => valid input, otherwise get new line
			mr->linenmbr = in_lineno[fndx];
			return 1;
		}
	}
}



//  The trace_read_pin_txt function reads the next valid trace record from the text,
//	moola format trace file WITHOUT data values.
//  The input buffer is then sent to 'process_moola_nodata' to decode and place the
//	information into the *mr record that was provided as an input.
//  0 is returned if the end of file was reached without completing a trace record
//	blank lines, comment lines, and invalid trace record lines are skipped

int32_t trace_read_pin_txt(int16_t fndx, memref *mr) {
	//	see list of static/global variables as well as these local variables
	char		*stat;				//	status response for fgets and process_moola_nodata
	crntfndx = fndx;				//	set for error messages to use
	while (1) {						//	loop until valid record or end-of-file
		stat = fgets(ibfr, 160, txtfil[fndx]);		//	get next line of input from text input file
		if (stat == NULL) {
			//	end of file reached
			return 0;
		}
		in_lineno[fndx]++;			//	increment line number
		stat = process_pin_nodata(mr);
		if (stat) {					//	stat != 0 => valid input, otherwise get new line
			mr->linenmbr = in_lineno[fndx];
			return 1;
		}
	}
}


/*
//  The trace_read_pin_valgz function reads the next valid trace record from the gzipped,
//	moola format trace file WITH data values
//  The input buffer is then sent to 'process_moola_data' to decode and place the
//	information into the *mr record that was provided as an input.
//  0 is returned if the end of file was reached without completing a trace record
//	blank lines, comment lines, and invalid trace record lines are skipped

int32_t trace_read_pin_valgz(int16_t fndx, memref *mr) {
	//	see list of static/global variables as well as these local variables
	char		*stat;				//	status response for gzgets and process_moola_data
	crntfndx = fndx;				//	set for error messages to use
	while (1) {						//	loop until valid record or end-of-file
		stat = gzgets(gzfil[fndx], ibfr, 160);		//	get next line of input from gzip input file
		if (stat == NULL) {
			//	end of file reached
			return 0;
		}
		in_lineno[fndx]++;			//	increment line number
		stat = process_pin_data(mr);
		if (stat) {					//	stat != 0 => valid input, otherwise get new line
			mr->linenmbr = in_lineno[fndx];
			return 1;
		}
	}
}



//  The trace_read_pin_valtxt function reads the next valid trace record from the text,
//	moola format trace file WITH data values
//  The input buffer is then sent to 'process_moola_data' to decode and place the
//	information into the *mr record that was provided as an input.
//  0 is returned if the end of file was reached without completing a trace record
//	blank lines, comment lines, and invalid trace record lines are skipped

int32_t trace_read_pin_valtxt(int16_t fndx, memref *mr) {
	//	see list of static/global variables as well as these local variables
	char		*stat;				//	status response for fgets and process_moola_data
	crntfndx = fndx;				//	set for error messages to use
	while (1) {						//	loop until valid record or end-of-file
		stat = fgets(ibfr, 160, txtfil[fndx]);		//	get next line of input from text input file
		if (stat == NULL) {
			//	end of file reached
			return 0;
		}
		in_lineno[fndx]++;			//	increment line number
		stat = process_pin_data(mr);
		if (stat) {					//	stat != 0 => valid input, otherwise get new line
			mr->linenmbr = in_lineno[fndx];
			return 1;
		}
	}
}



char *process_pin_data(memref *mr) {
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
		case 'i':	mr->oper = MRINSTR; mr->segmnt = INSTR; break;
		case 'L':
		case 'l':	mr->oper = MRREAD; break;
		case 'M':
		case 'm':	mr->oper = MRMODFY; break;
		case 'S':
		case 's':	mr->oper = MRWRITE; break;
		case 'P':
		case 'p':	mr->oper = XSTACK; break;
		case '#':	return NULL;	//	skip comment lines
		case '\n':	return NULL;	//	skip blank lines
			
		default:
			fprintf(stderr,
					"Unexpected trace record type, '%c', at column %ld in file %s line %lld\n%s",
					c, cptr - ibfr, in_fnames[crntfndx], in_lineno[crntfndx], ibfr);
			return NULL;
	}
	
	// #define DEBUG_INPUT
#ifdef DEBUG_INPUT
	//  help in debugging
	int64_t		lnmbr = 72190
	if (in_lineno[crntfndx] == lnmbr) {
		printf("Line found %d:\n", lnmbr);
	}
#endif
	c = *cptr++;				//	"consume" this character and go to next
	
	//	get the processor ID
	get_dec();					//	get a decimal value
	if (!cptr)	return NULL;	//	skip line if error in get_dec
	mr->pid = (int8_t) val;		//	save value as the processor ID
	
	//  get the hex address
	get_hex();
	if (!cptr)	return NULL;	//	skip line if error in get_hex
	mr->adrs = val;
	
	if (mr->oper == XSTACK || mr->oper == XFREE) {
		goto r_valid;
	}
	
	//	get the decimal size
	get_dec();
	if (!cptr)	return NULL;	//	skip line if error in get_dec
	mr->size = (int32_t) val;
	
	if (mr->oper == XALLOC) {
		goto r_valid;
	}
	
	//	if DATAVALS is not defined, then process_moola_data and process_moola_nodata are identical
#ifdef DATAVALS
	//	get the hex data values as bytes
	bytes = &mr->data[0];
	get_hexbytes();
	if (!cptr) 	return NULL;	//	skip line if error in get_hexbytes
#endif
	
	if (mr->oper == MRINSTR) {
		mr->segmnt = INSTR;
		goto r_valid;
	}
	
	SKIP_WS						//	get character code of memory segment and make it 0-4
								//	memory segment 0-4: global, heap, instruction, stack, other
	
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
	
r_valid:
	return 1;
}
*/


char *process_pin_nodata(memref *mr) {
	cptr = ibfr;				//	set to first character in trace record
	c = *cptr++;				//	get 1st char and point to next character
	SKIP_WS						//	white space allowed at start of line (not expected though)

	 //	get the thread ID
	 get_dec_pin();				//	get a decimal value
	 if (!cptr)	return NULL;	//	skip line if error in get_dec
	 // mr->pid = (int8_t) val;	//	save value as the processor ID field
	SKIP_WS						//	skip space between PID and trace type

	//	get the trace record type code (allow all codes - just because)
	switch (c) {
		case 'A':
		case 'a':	mr->oper = XALLOC; break;
		case 'F':
		case 'f':	mr->oper = XFREE; break;
		case 'I':
		case 'i':	mr->oper = MRINSTR; mr->segmnt = INSTR; break;
		case 'L':
		case 'l':	mr->oper = MRREAD; break;
		case 'M':
		case 'm':	mr->oper = MRMODFY; break;
		case 'R':
		case 'r':	mr->oper = MRREAD; break;
		case 'S':
		case 's':	mr->oper = MRWRITE; break;
		case 'W':
		case 'w':	mr->oper = MRWRITE; break;
		case 'P':
		case 'p':	mr->oper = XSTACK; break;
		case '#':	return NULL;	//	skip comment lines
		case '\n':	return NULL;	//	skip blank lines
			
		default:
			fprintf(stderr,
					"Unexpected trace record type, '%c', at column %ld in file %s line %lld\n%s",
					c, cptr - ibfr, in_fnames[crntfndx], in_lineno[crntfndx], ibfr);
			return NULL;
	}
	
	c = *cptr++;				//	"consume" this character and go to next
	//  get the hex address
	get_hex_pin();
	if (!cptr)	return NULL;	//	skip line if error in get_hex
	mr->adrs = val;
	
	if (mr->oper == XSTACK || mr->oper == XFREE) {
		goto r_valid;
	}
	
	//	get the decimal size
	get_hex_pin();
	if (!cptr)	return NULL;	//	skip line if error in get_dec
	mr->size = 8; // (int32_t) val;
	
	//	get the decimal map flag
	// get_dec_pin();
	// if (!cptr)	return NULL;	//	skip line if error in get_dec
	// mapit = (int32_t) val;

	mapit = 1;

	if (mapit > 1) {
		fprintf(stderr,
				"Unexpected trace map value, '%d', at column %ld in file %s line %lld\n%s",
				mapit, cptr - ibfr, in_fnames[crntfndx], in_lineno[crntfndx], ibfr);
		return NULL;

	}

	/*
	if (mapit == 1) {
		mr->adrs += data_offs[mr->pid];		//	add offset for mapping addresses for this thread
	}
	*/
	
	if (mr->oper == XALLOC) {
		goto r_valid;
	}
	
	if (mr->oper == MRINSTR) {
		mr->segmnt = INSTR;
		goto r_valid;
	} else {
		mr->segmnt = OTHER;
	}

	goto r_valid;				//	no further info for pintool format
	
	
	SKIP_WS						//	get character code of memory segment and make it 0-4
								//	memory segment 0-4: global, heap, instruction, stack, other

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
	
r_valid:
	return 1;
}



//  Various support functions used in the two process_moola_ functions:

void get_dec_pin(void) {		//  get decimal value from input trace line
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




void get_hex_pin(void) {		//  get hexadecimal value from input trace line with required 0x lead
	SKIP_WS
	c2 = *cptr++;
	if (c != '0'  ||  c2 != 'x') {
		fprintf(stderr, "ERROR:  expecting hexadecimal leading '0x' character %ld, file %s, line %lld found '%c%c'\n%s",
				cptr-ibfr, in_fnames[crntfndx], in_lineno[crntfndx], c, c2, ibfr);
		cptr = NULL;		//  flag error
	}
	c = *cptr++;			//  get first char of hex value
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


void get_hexbytes_pin(void) {	//  get hexadecimal value from input trace line as sequence of bytes
							//  val contains the just read size
	for (bndx = 0; bndx < val; bndx++) {
		bytes[bndx] = 0;
		SKIP_WS
		//	special workaround for Gleipnir bug for 16-byte values
		//if (val == 16) cptr += 6; //  skip bogus leading zeros
		if (val == 16) cptr += 4; //  skip bogus leading zeros - skipping too many 1/14/2015 cfs
		if (!ishex[c]) {
			fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
					cptr-ibfr, in_fnames[crntfndx], in_lineno[crntfndx], c, ibfr);
			cptr = NULL;		//  flag error
			break;
		} else {
			bytes[bndx] = char_val[c];
			c = *cptr++;
			if (!ishex[c]) {
				//fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
				//		cptr-ibfr, in_fnames[crntfndx], in_lineno[crntfndx], c, ibfr);
				//cptr = NULL;		//  flag error
				break;
			} else {
				bytes[bndx] = bytes[bndx] * 16 + char_val[c];
				c = *cptr++;
			}
		}
	}
	return;
}
