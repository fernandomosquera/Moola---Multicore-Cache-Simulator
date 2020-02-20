//
//  utils.c  (common utility functions for Moola Multicore Cache Simulator)
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
////////////////////////////////////////////////////////////////////////////////
//
//  This is a file of utility functions for the moola cache simulator.
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <execinfo.h>
#include "moola.h"



//	clr_bit		clears the bit at the designated position within an array of bytes.
//				The input array implements a bit array of size N by using a byte
//				array of size N/8.  The bit index is divided by 8 to select a single
//				byte and a mask is created by shifting a 1 to the correct position by the
//				bit index modulo 8.  The mask is inverted and ANDed with the byte value
//				which clears the desired bit.
inline
void clr_bit(int8_t *aray, int16_t bit) {
	int16_t		byte;		//	byte index into array
	int8_t		mask;		//	byte value
	int16_t		bit_id;		//	bit within the selected byte
	
	byte = bit >> 3;
	bit_id = bit & 7;
	mask = 1 << bit_id;
	aray[byte] = (mask ^ 0xff) & aray[byte];
}



void error(char * msg, int rval) {
	void	*callstack[128];
	printf("%s \n", msg);
	//backtrace_symbols_fd(callstack, 128, stderr);
	backtrace_symbols_fd(callstack, 128, 1);
	exit(rval);
}


//	This function accepts a pointer to memref instance and puts it on the
//	memref free list.  Perhaps it should be a preprocessor macro?
void free_memref(memref *mr) {
	mr->next = free_mrs;
	free_mrs = mr;
	//	if (mr == (void *) 0x0100106e60) {
	//		printf("mr 0x0100106e60 freed\n");
	//	}
	return;
}



//	get_bit		extracts a designated bit from an array of bytes
//				The input array implements a bit array of size N by using a byte
//				array of size N/8.  The bit index is divided by 8 to select a single
//				byte and then the appropriate bit is extracted by shifting by the
//				bit index modulo 8 and masking the final bit
inline
int8_t get_bit(int8_t *aray, int16_t bit) {
	int16_t	byte;		//	byte index into array
	int8_t		byte_val;	//	byte value
	int16_t	bit_id;		//	bit within the selected byte
	
	byte = bit >> 3;
	bit_id = bit & 7;
	byte_val = aray[byte];
	return (byte_val >> bit_id) & 1;
}




//  This function gets a memref instance.  It first checks to see if there
//	is a memref on the free list and returns from there.  If the free list
//	is empty, malloc() is called to get memory to make a new memref and that
//	is then returned.
memref *get_memref() {
	memref		*mr;			//	pointer to record that will be returned
	int16_t		i;
	
	if (free_mrs == NULL) {
		mr = malloc(sizeof(memref));
		if (mr == NULL) {
			error("Unable to allocate memory for 'get_memref' request", -10);
		}
	} else {
		mr = free_mrs;
		free_mrs = mr->next;
	}
	//	if (mr == (void *) 0x0100106e60) {
	//		printf("mr 0x0100106e60 gotten\n");
	//	}
	mr->adrs = 0;
	mr->linenmbr = 0;
	mr->next = 0;
	mr->oper = 0;
	mr->pid = 0;
	mr->prev = 0;
	mr->segmnt = 0;
	mr->size = 0;
	mr->split = 0;
	mr->time = 0;
	for (i = 0; i < MAX_MR_DATA; i++) {
		mr->data[i] = 0;
	}
	return mr;
}



//	halloc		This function tracks heap addresses that have been allocated
void halloc(memref *mr){
	return;
	
}



//	hfree		This function tracks heap addresses that have been freed
void hfree(memref *mr) {
	return;
	
}



//	int64_to_str	converts an integer to a group of comma separated triads
char		*int64_to_str(int64_t val, char *str) {
	//  maximum 64 bits => 19 digits possible, or 6 triads plus 1 leading digit
	uint16_t	chr_cnt;	//	number of characters written
	uint16_t	vals[7];	//	values of triads
	int16_t		v;			//	index for triads
	int16_t		triads;		//	count of triads
	int64_t		tmp;		//	initial time value
	
	if (val < 0) {
		tmp = val * -1l;		//	make positive
		sprintf(str, "-");	//	print sign
		chr_cnt = 1;
	} else {
		tmp = val;
		chr_cnt = 0;
	}
	triads = 0;
	for (v = 0; v < 7; v++) {
		vals[v] = (uint16_t) (tmp % 1000);
		tmp = tmp / 1000;
		if (tmp == 0) {
			break;			//  nothing left to separate
		}
		triads++;
	}
	chr_cnt += sprintf(str, "%d", vals[triads]);
	for (v = triads-1; v >= 0; v--) {
		chr_cnt += sprintf(str+chr_cnt, ",%03d", vals[v]);
	}
	return str;
}



//	print_cache			prints the cache statistics
//		cash			pointer to the cache to output
void		print_cache(cache *cash) {
	cache_cfg	*cfg = cash->config;
	int32_t		kbyte = 1024;
	int32_t		mbyte = 1024 * 1024;
	char		bfr1[24];
	char		bfr2[24];
	char		bfr3[24];
	char		bfr4[24];
	char		bfr5[24];
	char		bfr6[24];
	char		bfr7[24];
	int16_t		seg;		//	loop through the segments
	int32_t		set;		//	loop through the sets
	int16_t		res;		//	loop through the resolutions
	int16_t		typ;		//	loop through the types
	char		*seg_str[5] = {"global", "heap  ", "instr ", "stack ", "other "};
	char		*res_str[4] = {"byte   ", "element", "sub blk", "block  "};
	
	
	printf("\n\nMoola simulation statistics for cache:  %s\n\n", cash->name);
	if (cfg->size >= mbyte) {
		printf("Size:  %4d MB,  ", cfg->size / mbyte);
	} else if (cfg->size >= kbyte) {
		printf("Size:  %4d KB,  ", cfg->size / kbyte);
	} else {
		printf("Size:  %4d  B,  ", cfg->size);
	}
	printf("Block size:  %d  ", cfg->lin_siz);
	printf("Subblock:  %d  ", cfg->sb_siz);
	printf("Associativity:  %d\n", cfg->assoc);
	printf("Shared: %c,  Prefetch: %c,  Replacement: %c,  Walloc: %c,  Write: %c\n\n",
		   cfg->shared, cfg->pref_pol, cfg->replace, cfg->walloc_pol, cfg->write_pol);
	printf("ALL               TOTAL          INSTRUCTION          DATA READ");
    printf("         DATA WRITE         PREF INSTR          PREF DATA          SPLIT REFS\n");
	printf("Fetches:  %16s,  %16s,  %16s,  %16s,  %16s,  %16s,  %16s\n",
		   int64_to_str(cash->fetch[MRINSTR] + cash->fetch[MRREAD] + cash->fetch[MRWRITE]
						+ cash->fetch[PRINSTR] + cash->fetch[PRREAD], bfr1),
		   int64_to_str(cash->fetch[MRINSTR], bfr2), int64_to_str(cash->fetch[MRREAD], bfr3),
		   int64_to_str(cash->fetch[MRWRITE], bfr4), int64_to_str(cash->fetch[PRINSTR], bfr5),
		   int64_to_str(cash->fetch[PRREAD], bfr6), int64_to_str(cash->split_blk, bfr7));
	printf("Misses:   %16s,  %16s,  %16s,  %16s,  %16s,  %16s\n",
		   int64_to_str(cash->miss[MRINSTR] + cash->miss[MRREAD] + cash->miss[MRWRITE]
						+ cash->miss[PRINSTR] + cash->miss[PRREAD], bfr1),
		   int64_to_str(cash->miss[MRINSTR], bfr2), int64_to_str(cash->miss[MRREAD], bfr3),
		   int64_to_str(cash->miss[MRWRITE], bfr4), int64_to_str(cash->miss[PRINSTR], bfr5),
		   int64_to_str(cash->miss[PRREAD], bfr6));
	//  TBD:  repeat for capacity, compulsory, conflict, coherency
	//	TBD:  should these be split into segments?
	
	printf("\nMem Seg  Resolution     Read           Untouch              Live");
	printf("           Useless             Dusty              Dead             Mixed\n");
	for (seg = 0; seg < 5; seg++) {
		for (res = 0; res < 4; res++) {
			printf("%s   %s", seg_str[seg], res_str[res]);
			for (typ = 0; typ < 7; typ++) {
				printf("%16s  ", int64_to_str(cash->cntrs[seg][res][typ], bfr1));
			}
			printf("\n");
		}
	}
	
	//	print details of each set if requested
	if (cash->config->write_sets) {
		printf("Set    Counter          Global        Heap");
		printf("	   Instruction           Stack            Other\n");
		for (set = 0; set < cash->nmbr_sets; set++) {
			printf("%6d  access", set);
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].access[seg], bfr1));
			}
			printf("\n        fetch ");
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].fetch[seg], bfr1));
			}
			printf("\n        hit   ");
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].hits[seg], bfr1));
			}
			printf("\n        miss  ");
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].misses[seg], bfr1));
			}
			printf("\n        wrback");
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].wrback[seg], bfr1));
			}
			printf("\n        evict ");
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].evict[seg], bfr1));
			}
			printf("\n        clean ");
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].clean[seg], bfr1));
			}
			printf("\n        nvalid");
			for (seg = 0; seg < 5; seg++) {
				printf("%16s,  ", int64_to_str(cash->sets[set].nvalid[seg], bfr1));
			}
			printf("\n");
		}
	}
}



//	This function prints a copy of the memref as it looked in the input file
void	print_mr(memref *mr) {
	char	*oper_str = "LSMIAFP";
	
	printf("%4lld: %4lld:   ", mr->linenmbr, mr->time);
	switch (mr->oper) {
		case MRREAD:
		case MRWRITE:
		case MRMODFY:
			printf("%c %2d %011llx %d ", oper_str[mr->oper], mr->pid, mr->adrs, mr->size);
			for (int i = 0; i < mr->size; i++) {
				printf("%02x", mr->data[i]);
				if (i % 4 == 3  &&  i != mr->size - 1) {
					printf(" ");
				}
			}
			printf(" %c\n", seg_code[mr->segmnt]);
			break;
			
		case MRINSTR:
			printf("I %2d %011llx %d ", mr->pid, mr->adrs, mr->size);
			for (int i = 0; i < mr->size; i++) {
				printf("%x", mr->data[i]);
			}
			printf("\n");
			break;
			
		case XALLOC:
			printf("A %2d %011llx %d\n", mr->pid, mr->adrs, mr->size);
			break;
			
		case XFREE:
			printf("F %2d %011llx\n", mr->pid, mr->adrs);
			break;
			
		case XSTACK:
			printf("P %2d %011llx\n", mr->pid, mr->adrs);
			break;
			
		default:
			printf("Unrecognized MR operation:  %d\n", mr->oper);
			break;
	}
}



//			print_set	prints one cache-line set from MRU to LRU, giving adrs and pointers
void		print_set(cacheset *set) {
	cacheline	*cl;			//	pointer to cacheline to print
	int32_t		nmbr_lines;		//	number of lines in the set
	int32_t		n;				//	loop index for the lines
	
	nmbr_lines = set->owner->assoc;
	printf("set MRU line:  %03x, tag %10llx\n", (uint16_t) set->mru & 0xfff, set->mru->adrs);
	printf("set LRU line:  %03x, tag %10llx\n", (uint16_t) set->lru & 0xfff, set->lru->adrs);
	cl = set->mru;
	for (n = 0; n < nmbr_lines; n++) {
		printf("line memory:   %03x, tag %10llx,  mru %03x,  lru %03x\n", (uint16_t) cl & 0xfff,
			   cl->adrs, (uint16_t) cl->mru & 0xfff, (uint16_t) cl->lru & 0xfff);
		cl = cl->lru;
		fflush(stdout);
	}
	printf("\n");
	fflush(stdout);
}



//	print_set_stats		prints the statistics of each set of the givien cache
//		cash			pointer to the cache having the sets to output
void print_set_stats(cache *cash) {
	
}



//	put_bit		inserts a bit into a designated position within an array of bytes.
//				The input array implements a bit array of size N by using a byte
//				array of size N/8.  The bit index is divided by 8 to select a single
//				byte and a mask is created by shifting a 1 to the correct position by the
//				bit index modulo 8.  If the input value is 0, the mask is inverted and
//				ANDed with the byte value.  If the input value is 1, the mask is ORed
//				with the byte value
inline
void put_bit(int8_t val, int8_t *aray, int16_t bit) {
	int16_t	byte;		//	byte index into array
	int8_t		mask;		//	byte value
	int16_t	bit_id;		//	bit within the selected byte
	
	byte = bit >> 3;
	bit_id = bit & 7;
	mask = 1 << bit_id;
	if (val == 0) {
		aray[byte] = (mask ^ 0xff) & aray[byte];
	} else {
		aray[byte] = mask | aray[byte];
	}
}



//	This function accepts a processor ID and a memref pointer and then adds
//	the memref item to the appropriate processor queue
void	queue_add(int16_t pid, memref *mr) {
	if (queues[pid].count == 0) {			//	queue currently empty
		queues[pid].count = 1;				//	initialize item count
		queues[pid].newest = mr;			//	both newest and oldest are this item
		queues[pid].oldest = mr;
		mr->next = NULL;					//	forward link of item being added
		mr->prev = NULL;					//	backward link of item being added
	} else {
		queues[pid].count++;				//	increment item count
		mr->next = queues[pid].newest;		//	forward link of item being added
		mr->prev = NULL;					//	backward link of item being added
		queues[pid].newest->prev = mr;		//	backward link of current newest
		queues[pid].newest = mr;			//	item being added is now newest
	}
}



//	This function accepts a processor ID and removes the oldest memref item for
//	that processor and returns its pointer.  It returns NULL if the queue is empty
memref	*queue_take(int16_t pid) {
	memref		*mr;		//	pointer to return
	if (queues[pid].count == 0) {
		error("queue_take called when selected queue was empty", -11);
		mr = NULL;
	} else {
		mr = queues[pid].oldest;			//	get oldest queue item
		if (queues[pid].count == 1) {		//	special needs for single element
			queues[pid].count = 0;			//	indicate empty queue
			queues[pid].newest = NULL;		//	newest item is NULL
			queues[pid].oldest = NULL;		//	oldest item is NULL
		} else {
			queues[pid].count--;			//	decrement item count
			queues[pid].oldest = mr->prev;	//	oldest now points to previous
			mr->prev->next = NULL;			//	mark previous as new end of list			
		}
	}
	return mr;
}



//	set_bit		sets the bit at the designated position within an array of bytes.
//				The input array implements a bit array of size N by using a byte
//				array of size N/8.  The bit index is divided by 8 to select a single
//				byte and a mask is created by shifting a 1 to the correct position by the
//				bit index modulo 8.  The mask is ORed with the byte value
inline
void set_bit(int8_t *aray, int16_t bit) {
	int16_t	byte;		//	byte index into array
	int8_t		mask;		//	byte value
	int16_t	bit_id;		//	bit within the selected byte
	
	byte = bit >> 3;
	bit_id = bit & 7;
	mask = 1 << bit_id;
	aray[byte] = mask | aray[byte];
}




