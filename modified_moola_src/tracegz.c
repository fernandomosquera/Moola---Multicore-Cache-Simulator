//
//  tracegz.c
//  moola
//
//  Created by Charles Shelor on 1/30/13.
//  Copyright (c) 2013 Charles Shelor. All rights reserved.
//

#include <stdio.h>
#include <stdint.h>
#include "zlib.h"

#include "moola.h"


//  The inputs to tracegz are an index to an open gzipped trace file
//  and a pointer to a memory record to fill with data from the trace file.
//  The function returns 0 if the trace information was valid and filled in.
//  The function returns 1 if an error was encountered and returns 2 if the
//  end of file was reached.
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
uint8_t		*bytes;			//	pointer to data value bytes (function global)
int16_t		bndx;			//	index to the current byte of data (function global)
char		c;				//  character from input trace file (function global)
char		*cptr;			//	pointer to next character in ibfr (function global)
char		*gzfname[8];	//	pointer to input filenames (static)
int16_t		gzfndx;			//	current file index for use by sub function error reporting
int64_t		gzlineno[8];	//	line number of most recently read line in the file (static)
char		ibfr[160];		//	buffer for input, 160 should be long enough for each line (function global)
gzFile		*ifil[8];		//	pointer to gzipped trace file for input (static)
int64_t		val;			//	temporary generation of an input value (function global)

//  These macros and in-line functions improve code readability by abstracting these common actions
#define FIND_WS 	while (c != ' ' && c != '\t' && c != '\n') { c = *cptr++; }
#define SKIP_WS 	while (c == ' ' || c == '\t') { c = *cptr++; }

void inline get_dec(void);		//  get decimal value from input trace line
void inline get_hex(void);		//  get hexadecimal value from input trace line
void inline get_hexbytes(void);	//  get hexadecimal value from input trace line as sequence of bytes


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



//  Call this function to close the trace file
void close_trace_gz(int16_t fil) {
	gzclose(ifil[fil]);
	return;
}



//  Call this function to open the trace file
int32_t open_trace_gz(int16_t fndx, char * fname) {
	
	ifil[fndx] = gzopen(fname, "r");
	gzfname[fndx] = fname;
	if (ifil[fndx] == NULL) {
		printf("Error opening file '%s' for trace input.\n", fname);
		return 0;
	} else {
		gzlineno[fndx] = 0;
		return 1;
	}
	
}


//  Call this function to reopen the trace file
int32_t reopen_trace_gz(int16_t fndx, char * fname) {
	
	ifil[fndx] = gzopen(fname, "r");
	gzfname[fndx] = fname;
	if (ifil[fndx] == NULL) {
		printf("Error opening file '%s' for trace input.\n", fname);
		return 0;
	} else {
		return 1;
	}
	
}


//  This function reads the next valid trace record
//	The information is placed in the *mr record
//	The current line number of the file is returned on normal completion
//  0 is returned if the end of file was reached without completing a trace record
//	blank lines, comment lines, and invalid trace record lines are skipped

int32_t tracegz(int16_t fndx, memref *mr) {
	
	//	see list of static/global variables as well as these local variables
	//union word_bytes {				//	union for converting between bytes/words
	//	int8_t		bytes[8];
	//	int64_t	word;
	// } mem;
	char		*stat;				//	status response for gzgets
	gzfndx = fndx;
	
	while (1) {						//	loop until valid record or end-of-file
									//  TBD set mr defaults here?
		stat = gzgets(ifil[fndx], ibfr, 160);		//	get next line of input from gzip input file
		if (stat == NULL) {
			//	end of file reached
			return 0;
		}
		gzlineno[fndx]++;			//	increment line number
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
			case '#':	continue;	//	skip comment lines
			case '\n':	continue;	//	skip blank lines
				
			default:
				fprintf(stderr,
						"Unexpected trace record type, '%c', at column %ld in file %s line %lld\n%s",
						c, cptr - ibfr, gzfname[fndx], gzlineno[fndx], ibfr);
						continue;
		}
		
		c = *cptr++;				//	"consume" this character and go to next
		
		//	get the processor ID
		get_dec();					//	get a decimal value
		if (!cptr)	continue;		//	skip line if error in get_dec
		mr->pid = (int8_t) val;		//	save value as the processor ID
		
		//  get the hex address
		get_hex();
		if (!cptr)	continue;		//	skip line if error in get_hex
		mr->adrs = val;
		
		if (mr->oper == XSTACK || mr->oper == XFREE) {
			goto r_valid;
		}
		
		//	get the decimal size
		get_dec();
		if (!cptr)	continue;		//	skip line if error in get_dec
		mr->size = (int32_t) val;
		
		if (mr->oper == XALLOC) {
			goto r_valid;
		}
		
		//	get the hex data values as bytes
		bytes = &mr->data[0];
		get_hexbytes();
		if (!cptr) 	continue;		//	skip line if error in get_hexbytes
		
		if (mr->oper == MRINSTR) {
			goto r_valid;
		}
		
		SKIP_WS					//	get character code of memory segment and make it 0-4
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
		mr->linenmbr = gzlineno[fndx];
		return 1;
	}

	return -1;				//	this should never get returned
}


void get_dec(void) {		//  get decimal value from input trace line
	SKIP_WS
	val = 0;
	if (!isdec[c]) {
		fprintf(stderr, "ERROR:  expecting decimal digit at character %ld, file %s, line %lld found '%c'\n%s",
				cptr-ibfr, gzfname[gzfndx], gzlineno[gzfndx], c, ibfr);
		cptr = NULL;		//  flag error
	} else {
		while (isdec[c]) {		//	first non-decimal character terminates the value
			val = val * 10 + char_val[c];
			c = *cptr++;
		}
	}
	return;
}




void get_hex(void) {		//  get hexadecimal value from input trace line
	SKIP_WS
	val = 0;
	if (!ishex[c]) {
		fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
				cptr-ibfr, gzfname[gzfndx], gzlineno[gzfndx], c, ibfr);
		cptr = NULL;		//  flag error
	} else {
		while (ishex[c]) {		//	first non-hex character terminates the value
			val = val * 16 + char_val[c];
			c = *cptr++;
		}
	}
	return;
}


void get_hexbytes(void) {	//  get hexadecimal value from input trace line as sequence of bytes
									//  val contains the just read size
	for (bndx = 0; bndx < val; bndx++) {
		bytes[bndx] = 0;
		SKIP_WS
		if (!ishex[c]) {
			fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
					cptr-ibfr, gzfname[gzfndx], gzlineno[gzfndx], c, ibfr);
			cptr = NULL;		//  flag error
			break;
		} else {
			bytes[bndx] = char_val[c];
			c = *cptr++;
			if (!ishex[c]) {
				fprintf(stderr, "ERROR:  expecting hexadecimal digit at character %ld, file %s, line %lld found '%c'\n%s",
						cptr-ibfr, gzfname[gzfndx], gzlineno[gzfndx], c, ibfr);
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



//  simple test file generator
void create_test_gzfile() {
	
	int32_t		i;				//	loop variable
	gzFile		*ofil;			//	file pointer for tmp output file
	int32_t		p;				//	processor index
	int32_t		status;			//	information returned from gzbuffer function
	
	ofil = gzopen("test_trace.gz", "wb9");		//	open output file with max compression
	status = gzbuffer(ofil, 16384);				//	set compression buffer size
												//	write some simple stuff
	gzprintf(ofil, "I 0 babe4ace00 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "L 0 deadfee110 4 8badf00d G\n");
	gzprintf(ofil, "L 0 deadfee114 4 cafe4d06 G\n");
	gzprintf(ofil, "I 0 babe4ace08 8 b1b2b3b4b5b6b7b8\n");
	gzprintf(ofil, "M 0 deadfee118 8 0102030405060708 G\n");
	gzprintf(ofil, "I 0 babe4ace10 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "P 0 1deadface00\n");
	gzprintf(ofil, "L 0 deadfee1fc 8 1112131415161718 G\n");		//	test split reference

	gzprintf(ofil, "I 1 defecafe00 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "  L  1  b1ade40  4   12345678  H   \n");		//	test spaces
	gzprintf(ofil, "I 1 defecafe08 8 b1b2b3b4b5b6b7b8\n");
	gzprintf(ofil, "P 1 1deadface18\n");
	gzprintf(ofil, "I 1 defecafe10 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "L 1 b1ade44 8 2425262728292a2b H\n");

	gzprintf(ofil, "I 2 feed4ace00 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "S 2 babefee110 8 abaddea14dad0123 S\n");
	gzprintf(ofil, "I 2 feed4ace08 8 b1b2b3b4b5b6b7b8\n");
	gzprintf(ofil, "S 2 beadfee110 8 2425262728292a2b S\n");
	gzprintf(ofil, "I 2 feed4ace10 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "S 2 deedfee110 8 8765432187654321 S\n");
	gzprintf(ofil, "I 2 feed4ace18 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "S 2 deaffee110 8 6543218765432187 S\n");		//	force evict from 4-way set
	gzprintf(ofil, "I 2 feed4ace18 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "S 2 fadefee110 8 4321876543218765 S\n");		//	force evict from 4-way set

	gzprintf(ofil, "I 3 facec00100 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "A 3 2000000 256\n");
	gzprintf(ofil, " S 3  b1ade40  4   01234567  H   \n");
	gzprintf(ofil, "I 3 facec00108 8 b1b2b3b4b5b6b7b8\n");
	gzprintf(ofil, "S 3 b1ade44 8 25262728292a2b2c H\n");
	gzprintf(ofil, "I 3 facec00110 8 a1a2a3a4a5a6a7a8\n");
	gzprintf(ofil, "F 3 2000000 256\n");
	p = 27;
	for (i = 4; i <= 22; i++) {					//	make file contain data for 4 procs
		gzprintf(ofil, "I %d %X 8 %016X\n",   i%4, i * p++ * 22340 * 8, i * p++ * 23490);
		gzprintf(ofil, "I %d %X 8 %016X\n",   i%4, i * p++ * 22341 * 8, i * p++ * 23480);
		gzprintf(ofil, "S %d %X 8 %016X H\n", i%4, i * p++ * 12342 * 8, i * p++ * 23470);
		gzprintf(ofil, "L %d %X 8 %016X H\n", i%4, i * p++ * 12343 * 8, i * p++ * 23460);
		gzprintf(ofil, "I %d %X 8 %016X\n",   i%4, i * p++ * 22342 * 8, i * p++ * 23450);
		gzprintf(ofil, "L %d %X 8 %016X H\n", i%4, i * p++ * 12345 * 8, i * p++ * 23440);
		gzprintf(ofil, "I %d %X 8 %016X\n",   i%4, i * p++ * 22343 * 8, i * p++ * 23430);
		gzprintf(ofil, "S %d %X 8 %016X H\n", i%4, i * p++ * 12347 * 8, i * p++ * 23420);
	}
	//	Let's get some cache hits now
	gzprintf(ofil, "I 0 babe4ace00 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "L 0 deadface00 8 8badf00dcafe4d06 G\n");
	gzprintf(ofil, "I 0 babe4ace08 8 b1b2b3b4b5b6b7b8  \n");
	gzprintf(ofil, "L 0 deadface20 8 0102030405060708 G\n");
	gzprintf(ofil, "I 0 babe4ace10 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "P 0 1deadface00\n");
	gzprintf(ofil, "L 0 deadface28 8 1112131415161718 G\n");
	
	gzprintf(ofil, "I 1 defecafe00 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "  L  1  b1ade40  4   12345678  H   \n");		//	test spaces
	gzprintf(ofil, "I 1 defecafe08 8 b1b2b3b4b5b6b7b8  \n");
	gzprintf(ofil, "P 1 1deadface18\n");
	gzprintf(ofil, "I 1 defecafe10 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "L 1 b1ade44 8 2425262728292a2b H\n");
	
	gzprintf(ofil, "I 2 feed4ace00 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "S 2 1deadface00 8 8badf00dcafe4d06 S\n");
	gzprintf(ofil, "I 2 feed4ace08 8 b1b2b3b4b5b6b7b8  \n");
	gzprintf(ofil, "S 2 1deadface20 8 28292a2b24252627 S\n");
	gzprintf(ofil, "I 2 feed4ace10 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "S 2 1deadface28 8 1234567812345678 S\n");
	
	gzprintf(ofil, "I 3 facec00100 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "A 3 2000000 256\n");
	gzprintf(ofil, " S 3  b1ade40  4   01234567  H   \n");
	gzprintf(ofil, "I 3 facec00108 8 b1b2b3b4b5b6b7b8  \n");
	gzprintf(ofil, "S 3 b1ade44 4 25262728 H\n");
	gzprintf(ofil, "I 3 facec00110 8 a1a2a3a4a5a6a7a8  \n");
	gzprintf(ofil, "F 3 2000000 256\n");
	for (i = 23; i <= 28; i++) {					//	make file contain data for 3 procs
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%3, i * p++ * 22340 * 8, i * p++ * 23490);
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%3, i * p++ * 22341 * 8, i * p++ * 23480);
		gzprintf(ofil, "S %d %X 8 %016X H\n", i%3, i * p++ * 12342 * 8, i * p++ * 23470);
		gzprintf(ofil, "L %d %X 8 %016X H\n", i%3, i * p++ * 12343 * 8, i * p++ * 23460);
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%3, i * p++ * 22342 * 8, i * p++ * 23450);
		gzprintf(ofil, "L %d %X 8 %016X H\n", i%3, i * p++ * 12345 * 8, i * p++ * 23440);
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%3, i * p++ * 22343 * 8, i * p++ * 23430);
		gzprintf(ofil, "S %d %X 8 %016X H\n", i%3, i * p++ * 12347 * 8, i * p++ * 23420);
	}
	for (i = 29; i <= 33; i++) {					//	make file contain data for 2 procs
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%2, i * p++ * 22340 * 8, i * p++ * 23490);
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%2, i * p++ * 22341 * 8, i * p++ * 23480);
		gzprintf(ofil, "S %d %X 8 %016X H\n", i%2, i * p++ * 12342 * 8, i * p++ * 23470);
		gzprintf(ofil, "L %d %X 8 %016X H\n", i%2, i * p++ * 12343 * 8, i * p++ * 23460);
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%2, i * p++ * 22342 * 8, i * p++ * 23450);
		gzprintf(ofil, "L %d %X 8 %016X H\n", i%2, i * p++ * 12345 * 8, i * p++ * 23440);
		gzprintf(ofil, "I %d %X 8 %016X  \n", i%2, i * p++ * 22343 * 8, i * p++ * 23430);
		gzprintf(ofil, "S %d %X 8 %016X H\n", i%2, i * p++ * 12347 * 8, i * p++ * 23420);
	}
	gzclose(ofil);
	
}