//
//  reference.c  (primary cache model of Moola Multicore Cache Simulator)
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
//  This is the file containing functions specific to processing a memory reference
//	to a given cache and for processing cache commands from the processor.
//
//  The functions included are:
//      classify_miss	classify the cache miss: compulsory, capacity, conflict, coherence
//      clean_all		processor command to mark all cache lines as clean
//      cl_init			moola support function to initializes a cacheline before using it
//      is_dead			moola support function to determine if address is dead memory
//      invalidate_all	processor command to mark all cache lines as invalid
//      move2_lru		moola support function to make a cache line the LRU
//      move2_mru		moola support function to make a cacge line the MRU
//      print_cntrs		moola debugging function to print counter values
//      reference		implements a processor memory reference to a cache
//      ref_split		a reference that crosses cache lines calls this to create 2 references
//      search			searches a set of cache lines for a tag match making a "hit" or "miss"
//      update_cl		updates cache line structures to implement a cache line move between levels
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "moola.h"


int16_t classify_miss(cache *cash, memref *mr) {
	return 0;
}



//			clean_all		cleans all of the dirty lines in a cache
void		clean_all(cache *cash) {
	return;
}



//			cl_init			initializes a cacheline before being used
void		cl_init(cacheline *cl) {
	int16_t		i;		//	loop index for data bytes and stat bytes
	//	The cache line is already part of a set, just need to clear data
	//	and flags from any prior use
	cl->adrs = 0;
	for (i = 0; i < cl->owner->config->lin_siz; i++) {
		cl->data[i] = 0;
		cl->orig[i] = 0;
		cl->stat[i] = 0;
	}
	cl->valid = 0;
	cl->referncd = 0;
	cl->dirty = 0;
	cl->shared = 0;
	cl->exclusiv = 0;
	cl->oper = 0;
	cl->alloctime = 0;
	return;
}



//			invalidate_all	invalidates all lines in a cache
void		invalidate_all(cache *cash) {
	
}



//	is_dead			determine if the input address is a dead address
int8_t		is_dead(int64_t adrs, int8_t segment) {
	
	return 0;
}



//			move2_lru		move the input cacheline to the least recently used position
void		move2_lru(cacheset *set, cacheline *cl) {
	
	if (set->lru == cl) {
		return;					//	already LRU, so just return
	}
	
	//	The cache lines in a set form a dual linked ring.
	//  The first step is to remove the input cache line from the ring
	cl->lru->mru = cl->mru;
	cl->mru->lru = cl->lru;
	//  Now insert it after the current LRU
	if (set->mru == cl) {		//	is cl the current MRU line
		set->mru = cl->lru;		//	get new MRU line
	}
	cl->mru = set->lru;			//  line's more recent link goes to current LRU
	cl->lru = set->mru;			//	line's less recent link goes to current MRU
	set->lru->lru = cl;			//  place line after current LRU
	set->mru->mru = cl;			//  place line "before" current MRU
	set->lru = cl;				//	make line the current LRU
}



//			move2_mru		move the input cacheline to the most recently used position
void		move2_mru(cacheset *set, cacheline *cl) {
	
	if (set->mru == cl) {
		return;					//	already MRU, so just return
	}
	
	//	The cache lines in a set form a dual linked ring.
	//  The first step is to remove the input cache line from the ring
	cl->lru->mru = cl->mru;
	cl->mru->lru = cl->lru;
	//  Now insert it before the current MRU
	if (set->lru == cl) {		//	is cl the current LRU line
		set->lru = cl->mru;		//	get new LRU line
	}
	cl->mru = set->lru;			//  line's more recent link goes to current LRU
	cl->lru = set->mru;			//	line's less recent link goes to current MRU
	set->lru->lru = cl;			//  place line "after" current LRU
	set->mru->mru = cl;			//  place line before current MRU
	set->mru = cl;				//	make line the current MRU
}



//		print_cntrs		prints out current counter values for the segment to assist debugging
void	print_cntrs(char *msg, cache *cash, int sgmnt) {
	char		*segmnt_str[] = {"Global", "Heap  ", "Instr ", "Stack ", "Other "};
	char		*acs_str[] = {"reads", "untch", "lives", "usels", "dusts", "deads", "mixed"};
	int16_t		j;
	
	printf("ref.c: update_cntrs %s, %s\n", msg, cash->name);
	for (j = READ_ACS; j < MIXD_WR; j++) {
		printf("%s %s:  %10lld bytes,   %10lld elements,   %10lld subblks,   %10lld blocks\n",
			   segmnt_str[sgmnt], acs_str[j],
			   cash->cntrs[sgmnt][BYT_RES][j], cash->cntrs[sgmnt][LMT_RES][j],
			   cash->cntrs[sgmnt][SUB_RES][j], cash->cntrs[sgmnt][BLK_RES][j]);
	}
	printf("%s %s:                  ,                      ,   %10lld subblks,   %10lld blocks\n",
		   segmnt_str[sgmnt], acs_str[MIXD_WR],
		   cash->cntrs[sgmnt][SUB_RES][MIXD_WR], cash->cntrs[sgmnt][BLK_RES][MIXD_WR]);
}



//			reference		performs a cache reference from the processor or from a higher
//							level cache.  See discussion below for 3 types of invoking reference
//							The time of the reference completion is returned.

int64_t		reference(cache *cash, memref *mr, cacheline *cl) {
	
	int64_t		adrs_in;		//	input address of mr or cl
	int64_t		break_adrs;		//	set to value for a break point
	int64_t		break_lnmbr;	//	set to value for a break point
	int16_t		byte_id;		//	index of a byte into byte arrays
	int16_t		cblock;			//	cache block number for distributed cache array
	int64_t		crnt_time;		//	current time of recent action
	int64_t		duration;		//	accumulate time duration for this reference
	cacheline	*hit;			//	indicates cache hit when not NULL, miss when NULL
	int			main_mem;		//	flag set to 1 when cash is main memory pseudo cache
	char		*memtrace_str = "RWMIriafsn--";		//  memtrace access type output string
	int16_t		offset;			//	offset between cl->adrs and hit->adrs
	int8_t		oper;			//  operation to perform
	memref		*ref1;			//	pointer for first reference if this access must be split
	int8_t		ref_case;		//	indicate which type of reference call this is
	int64_t		ref_time;		//	local time of start of the cache reference
#ifdef DEBUG_REF
	char		sbfr[30];		//	buffer for comma'd number strings
	char		sbfr2[30];		//	buffer for comma'd number strings
#endif
	int8_t		segment;		//	memory segment code from input
	cacheset	*set;			//	pointer to matching set for input address
	int32_t		set_nmbr;		//	set number of mr input address
	int16_t		size;			//	number of bytes to transfer
	int64_t		stall;			//	duration of time stalled waiting for cache available
	int8_t		stat;			//	status byte for main memory fill from mr
	int64_t		tagadrs;		//	cache line address of mr input address first byte
	int64_t		tagadrs2;		//	cache line address of mr input address last byte
	cacheline	*victim;		//	cache line evicted for a cache miss
	int8_t		write_op;		//	set for writes, cleared for reads

#ifdef DEBUG_REF
	static int	msg_cntr = 0;	//	limit number of output messages
#endif

	
	
	//	There are three distinct forms for a call to reference.
	//	1.  mr != NULL and cl == NULL implies a processor reference to L1.  This might cross
	//		the cacheline boundary and must be split into 2 cache references if it does.
	//		If it is a miss then evict the LRU or appropriate line, clean it for the new address
	//		and send it and the current mr to the next lower level to see if it hits there.
	//		If it is a hit or after it returns from the miss reference then perform the
	//		requested operations and update the counters.
	//	2.	mr != NULL and cl != NULL implies a miss fill request from a higher level.  On a hit
	//		peform the fill operation including setting the original data values from the mr input
	//		if they haven't been set in both the hit line and the passed in line.  Only perform
	//		the mr write operation at L1, marking the appropriate bytes as dirty.
	//	3.  mr == NULL and cl != NULL implies a write-back eviction.  Since this is an inclusive
	//		cache organization, it should hit at the next level and only the operation
	//		on the lower level cacheline needs to be performed.  However, sometimes the line
	//		can be evicted from the shared lower level before the writeback of the upper level
	//		which is handled as a pass-through write-back.
	
	
	if (mr) {
		//	set debug breakpoints here when looking for specific addresses, line numbers, etc
		//  set breakpoint based on which cache level to debug or at the nested if to catch all levels
		break_adrs = 0x0004f80038;
		if (mr->adrs == break_adrs) {
			if (cash->level == 1) {
				break_adrs = break_adrs;
			} else if (cash->level == 2) {
				break_adrs = break_adrs;
			} else if (cash->level == 3) {
				break_adrs = break_adrs;
			} else {
				break_adrs = break_adrs;
			}
		}
		break_lnmbr = 877330;
		if (mr->linenmbr == break_lnmbr) {
			if (cash->level == 1) {
				break_lnmbr = break_lnmbr;
			} else if (cash->level == 2) {
				break_lnmbr = break_lnmbr;
			} else if (cash->level == 3) {
				break_lnmbr = break_lnmbr;
			} else {
				break_lnmbr = break_lnmbr;
			}
		}
		if (mr == (void *) 0x0100106e60) {
			break_lnmbr = break_lnmbr;
		}
		//  end of breakpoint area
	}
	if (cl) {
		//	set debug breakpoints here when looking for specific addresses, line numbers, etc
		//  set breakpoint based on which cache level to debug or at the nested if to catch all levels
		break_adrs = 0x0004f80038;
		if (cl->adrs == break_adrs) {
			if (cash->level == 1) {
				break_adrs = break_adrs;
			} else if (cash->level == 2) {
				break_adrs = break_adrs;
			} else if (cash->level == 3) {
				break_adrs = break_adrs;
			} else {
				break_adrs = break_adrs;
			}
		}
		//  end of breakpoint area
	}
	//  determine which of 3 case types, set reference start time
	if (mr != NULL  && cl != NULL) {
		ref_case = 2;		//	both non-null, so case type 2
		ref_time = mr->time;
	} else if (mr) {
		ref_case = 1;		//  mr != NULL, so cl must be NULL, so case 1
		ref_time = mr->time;
	} else if (cl) {
		ref_case = 3;		//  cl != NULL, so mr must be NULL, so case 3
		ref_time = cl->time;
	} else {
		ref_case = 0;		//	both NULL, so must be calling error
		ref_time = 0;
		error("Both mr and cl inputs to reference() are NULL", -17);
	}
	
#ifdef DEBUG_REF
	if (ref_case <= 2) {
		printf("reference entry case %d:  %s  mr->address %012llx,  size %d, access %s at %s\n",
			   ref_case, cash->name, mr->adrs, mr->size, mropers[mr->oper], int64_to_str(ref_time, sbfr));
	} else {
		printf("reference entry case 3:  %s  cl->address %012llx,  size %d, access %s at %s\n",
			   cash->name, cl->adrs, cl->owner->config->lin_siz, mropers[cl->oper],
			   int64_to_str(ref_time, sbfr));
	}
	if (mr  &&  mr->segmnt == HEAP  &&  msg_cntr < 100  &&  is_dead(mr->adrs, mr->segmnt)) {
		printf("Line %lld: %s heap address dead at %012llx\n", mr->linenmbr, cash->name, mr->adrs);
		msg_cntr++;
	}
#endif
	
	//	first determine if this reference is to main memory pseudo cache as this is
	//	used in multiple locations within reference()
	main_mem = (cash->lower == NULL);
	cacheline *cl2, *cl3;
	cl2 = mem.sets[0].lru;
	cl2 = mem.sets[0].mru;
	if (main_mem) {
		set = &cash->sets[0];
		cl2 = set->lru;
		cl3 = set->mru;
		hit = NULL;
		if (memtracefil) {
			fprintf(memtracefil, "%lld: %c 0x%llx\n", cl->time, memtrace_str[cl->oper], cl->adrs);
		}
	}
	
	//	determine cache block number for distributed cache or set to 0
	cblock = 0;
	if (cash->config->arch == 'd') {
		cblock = cblock;	//	replace with get_cblock(adrs) function when implemented
	}
	

	//	If this is a memory reference at L1 (case 1), see if it should be split into 2 references
	//	also set local variables to the active values from mr.  If not case 1, set local variables
	//	to the active values from cl.
	if (ref_case == 1) {
		
		//	Get cache line tag address for first and last bytes, if not equal, then split the reference
		tagadrs  =  mr->adrs                 & cash->tagmask;
		tagadrs2 = (mr->adrs + mr->size - 1) & cash->tagmask;
		
		//ref1 = NULL;									//	set to tell later if this gets split
		if (tagadrs != tagadrs2) {
			ref1 = ref_split(cash, mr);					//	split reference into 2: ref1 and updated mr
			cash->split_blk++;							//	increment split counter
			mr->time = reference(cash, ref1, NULL);		//	process first reference, update mr start time
			free_memref(ref1);
			tagadrs = mr->adrs & cash->tagmask;			//	set to second reference tag value
		}
		adrs_in = mr->adrs;
		oper = mr->oper;
		segment = mr->segmnt;
		size = mr->size;
	} else {
		adrs_in = cl->adrs;
		tagadrs = cl->adrs;
		oper = cl->oper;
		segment = cl->segment;
		size = cl->owner->config->lin_siz;
	}
	
	set_nmbr = (((int32_t) adrs_in) & cash->setmask) >> cash->log2blksize;
	set = &cash->sets[set_nmbr];
	set->access[segment]++;								//	increment access counter
	
#ifdef DEBUG_REF
	printf("input address %012llx maps to tag address %012llx and set number %d\n",
		   adrs_in, tagadrs, set_nmbr);
#endif

	//	see if tag matches a line in the set
	hit = search(cash, tagadrs, set_nmbr);
	

	//  see if this is special cache operation of clean or invalidate rather than memory reference
	if (oper == CLCLEAN  &&  !main_mem) {
		//  if not a hit, issue a warning
		if (hit == NULL) {
			printf("WARNING: cacheline clean for %lld resulted in cache miss\n", adrs_in);
		} else {
			cl_init(hit);
			move2_lru(set, hit);
			set->clean[segment]++;						//	increment clean counter
		}
#ifdef DEBUG_REF
		printf("reference exit:  clean done\n");
#endif
		cash->last_busy = ref_time + cash->config->control;
		return cl->time + cash->config->control;		//	return completion time for a control access
	}
	if (oper == CLNVALD  &&  !main_mem) {
		//  if not a hit, issue a warning
		if (hit == NULL) {
			printf("WARNING: cacheline invalidate for %lld resulted in cache miss\n", adrs_in);
		} else {
			hit->valid = 0;
			move2_lru(set, hit);
			set->nvalid[segment]++;						//	increment invalidate counter
		}
#ifdef DEBUG_REF
		printf("reference exit:  invalidate done\n");
#endif
		cash->last_busy = ref_time + cash->config->control;
		return ref_time + cash->config->control;		//	return completion time for a control access
	}
	
	//	initialize duration for this reference
	duration = cash->config->access;
	
	//	set stall value to any delay due to busy cache and update appropriate time field(s)
	if (cash->acss_time[cblock] > ref_time) {		//	stall is needed when true
		if (cash->config->arch == 'b'  ||  !hit  || (hit  &&  tagadrs == cash->miss_tag[cblock])) {
			stall = cash->miss_time[cblock] - ref_time;		//	stall until prior miss resolved
		} else {
			stall = cash->acss_time[cblock] - ref_time;		//	stall until prior access completed
		}
		cash->wait_time += stall;					//	update wait time stat accumulator
		ref_time += stall;							//  update start time of reference
	} else {										//  no stall is needed, update idle time stat
		cash->idle_time += ref_time - cash->last_busy;
		stall = 0;
	}
	
	crnt_time = ref_time + duration;
	cash->acss_time[cblock] = crnt_time;
	cash->miss_time[cblock] = crnt_time;
	cash->last_busy = crnt_time;
	
	if (mr) {
		mr->time = crnt_time;						//	start time for next level access if needed
	}
	if (cl) {
		cl->time = crnt_time;
	}
	
	
	if (hit) {
		set->hits[segment]++;						//	increment hit counter
	} else {
		set->misses[segment]++;						//	increment miss counter
		if (ref_case == 3) {
			//printf("WARNING: Inclusive cache violation.  Write back of %s %llx missed in %s at time %s\n",
			//	   cl->owner->name, cl->adrs, cash->name, int64_to_str(ref_time, sbfr));
		}
	}
	
	cash->fetch[oper]++;								//	update cache access counter
	
	write_op = (MRBASE(oper) == MRWRITE  ||  MRBASE(oper) == MRMODFY);

	if (hit == NULL) {
		cash->miss[oper]++;											//	update cache miss counter
		//	no match was found, need to	determine if evict a line, if so, get the victim
		victim = set->lru;											//	get LRU cache line in the set
		if (victim->valid != 0  &&  victim->dirty != 0) {			//	write-back is needed?
			victim->oper = MRWRITE;
			victim->time = crnt_time;
			if (!main_mem) {
				crnt_time = reference(cash->lower, NULL, victim);	//	write back data and mark time
				cash->last_busy = crnt_time;						//  mark cache as busy during write back
			}
			set->wrback[segment]++;									//	increment write back counter
		}
		if (victim->valid != 0) {
			set->evict[segment]++;									//	increment evict counter
		}
		cl_init(victim);											//	clean up cache line for reuse
		victim->time = crnt_time;
		move2_mru(set, victim);										//	make it most recently used
		victim->adrs = adrs_in & tagadrs;							//	setup address, operation, segment
		victim->oper = oper;
		victim->segment = segment;
		
		
		if (ref_case != 3) {
			//  case 3 is write back with miss, just update the victim with the data from cl input
			if (!main_mem) {
				if (mr != NULL) {
					mr->time = crnt_time;
				}
				crnt_time = reference(cash->lower, mr, victim);		//	fetch line from lower level
				cash->miss_tag[cblock] = tagadrs;					//	save tag of line being fetched
			} else {
				offset = (int16_t) (mr->adrs - victim->adrs);		//	use mr->data to init victim
				if (write_op) {
					stat = MRWRSTAT;
				} else {
					stat = MRRDSTAT;
				}
				for (byte_id = 0; byte_id < mr->size; byte_id++) {
					victim->data[offset + byte_id] = mr->data[byte_id];
					victim->orig[offset + byte_id] = mr->data[byte_id];
					victim->stat[offset + byte_id] = stat;
				}
				if (mr->split == 0) {
					victim->stat[offset] |= CBLEMNT;				//	mark as element start byte
				}
				victim->valid = 1;									//	TBD update for subblock tracking
			}
		}
		hit = victim;												//	can now process as a hit
		set->fetch[segment]++;										//	increment fetch counter
		cash->miss_time[cblock] = crnt_time;						//	update cache miss time
	}
	
	//  Do what you gotta do with the data now
	if (ref_case == 1) {				//	use mr->data to update hit->data
		update_cl(mr, NULL, hit);
	} else if (ref_case == 3) {
		update_cl(NULL, cl, hit);		//	use cl->data to update hit->data (write-back)
	} else {
		//	use hit->data to update cl->data (cache line fill)
		//	There is no need for counter updates or status checks
		//	just copy the data, orig, and stat bytes; adjusting for
		//	differences in cache line size if any.  Using update_cl()
		//	would require inserting flags to prevent counter updates
		//	and is much more complex than the simple copies here.
		//  note that fill operation is from lower level cache and that
		//	offset assumes lower level cache line is >= size of input
		//	cache line, thus the offset computations are reversed
		//	from the non-fill operations.
		offset = (int16_t) (cl->adrs - hit->adrs);
		for (byte_id = 0; byte_id < size; byte_id++) {
			cl->data[byte_id] = hit->data[offset + byte_id];
			cl->orig[byte_id] = hit->orig[offset + byte_id];
			cl->stat[byte_id] = hit->stat[offset + byte_id];
		}
		//	TBD need to update subblock status bits
		cl->valid = hit->valid;
		
	}
	
	//	set this cache's busy time until the reference completes
	cash->acss_time[cblock] = ref_time + duration;	//	cache busy from reference start to complete
#ifdef DEBUG_REF
		printf("reference exit %s  current time %s", cash->name, int64_to_str(crnt_time, sbfr));
		printf("  stall time %s, access time %s\n", int64_to_str(stall, sbfr), int64_to_str(duration, sbfr2));
	if (cash->level == 1) {
		printf("\n");
	}
#endif
	cash->last_busy = crnt_time;
	return crnt_time;
	
}   //  end reference



//  ref_split	Takes a memory reference that crosses a cache-line boundary
//				and splits it into 2 cache-line aligned references.
//				The reference to the first cache line is returned from the function
//				while the reference to the second cache line is returned by modification
//				of the input memref argument.
//	cash:		pointer to the cache being accessed, needed for cache-line size, etc
//	mr:			pointer to the memory reference data that is to be split
memref *ref_split(cache *cash, memref *mr) {
	int			i;				//	local loop index
	int32_t		orig_size;		//	save original size
	memref		*ref1;			//	pointer to the new reference being created
	int32_t		size1;			//	size of first reference
	int64_t		tagadrs;		//	cache line address of mr input address first byte
	int64_t		tagadrs2;		//	cache line address of mr input address last byte
	
	tagadrs = mr->adrs & cash->tagmask;
	tagadrs2 = (mr->adrs + mr->size - 1) & cash->tagmask;
	size1 = (int32_t) (tagadrs2 - mr->adrs);

	//  Move appropriate info from mr to ref1, adjusting as needed
	ref1 = get_memref();
	ref1->adrs = mr->adrs;			//	use initial address for ref1
	ref1->time = mr->time;			//	copy the time field
	ref1->linenmbr = mr->linenmbr;	//	copy the source file line number initiating this reference
	ref1->size = size1;				//	size of the first reference
	for (i = 0; i < size1; i++) {
		ref1->data[i] = mr->data[i];	//	copy the appropriate bytes and valid bits
	}
	for (i = size1; i < MAX_MR_DATA; i++) {
		ref1->data[i] = 0;				//	clear the remaining data bytes and valid bits
	}
	ref1->oper = mr->oper;
	ref1->segmnt = mr->segmnt;
	ref1->pid = mr->pid;
	ref1->split = 0;					//	mark as first memrec of split
	
	//	Now update mr address, size, data, and data valid
	orig_size = mr->size;
	mr->adrs += size1;
	mr->size -= size1;
	for (i = 0; i < mr->size; i++) {	//	copy the appropriate bytes and valid bits
		mr->data[i] = mr->data[i+size1];
	}
	for (i = mr->size; i < orig_size; i++) {
		mr->data[i] = 0;				//	clear the hang over data bytes and valid bits
	}
	mr->split = 1;						//	mark as second memrec of split
	
	/*  Currently don't see the need to count these
	//	Update split line counter
	switch (mr->segmnt) {
		case 'G':
			cash->split_block[0]++;
			break;
		case 'H':
			cash->split_block[1]++;
			break;
		case 'I':
			cash->split_block[2]++;
			break;
		case 'S':
			cash->split_block[3]++;
			break;
			
		default:
			error("Undefined memory segment", -19);
			break;
	}
	 */
	return ref1;
}


//	search		looks through the ways in the set to see if the input address matches the tag in the set
//				TBD subblocks not yet implemented (are subblocks even considered here?
cacheline  *search(cache *cash, int64_t tagadrs, int32_t set_nmbr) {
	int			way;
	cacheline	*cl;
	cacheset	*set;
	
	set = &cash->sets[set_nmbr];				//	set from set_nmbr
	cl = set->mru;								//	most recently used line in the set
	
	for (way = 0; way < cash->assoc; way++) {	//	test all lines in the set
		if (cl->valid  &&  cl->adrs == tagadrs) {
			return cl;
		}
		cl = cl->lru;							//	go to next less recently used line
	}
	return NULL;
}



//	update_cl	updates a cache line from either the mr input or the cl input
//				includes updating the dst->data, ->orig, and ->stat arrays.  Also
//				includes updating the cache access type counters.
//				Exactly 1 of mr/cl should be valid
void		update_cl(memref *mr, cacheline *cl, cacheline *dst) {
	
	int16_t		blk_dead;		//  count dead writes within the block
	int16_t		blk_dusty;		//  count dusty writes within the block
	int16_t		blk_live;		//  count live writes within the block
	int16_t		blk_read;		//  count reads within the block
	int16_t		blk_untouch;	//  count untouched bytes within the block
	int16_t		blk_usls;		//  count useless writes within the block
	cache		*cash;			//	pointer to cache containing destination line
	uint8_t		*data;			//	pointer to source current data
	uint8_t		dst_data;		//  destination data byte
	int16_t		dst_ndx;		//	index of byte into the destination data/orig/stat arrays
	int8_t		dst_valid;		//	indicate validity of destination byte
	int16_t		lmt_dead;		//  count dead writes within an element
	int16_t		lmt_dusty;		//  count dusty writes within an element
	int16_t		lmt_flag;		//	increment when element start found (2nd mr of split is not element)
	int16_t		lmt_live;		//  count live writes within an element
	int16_t		lmt_read;		//  count reads within an element
	int16_t		lmt_untouch;	//  count untouched bytes within an element
	int16_t		lmt_usls;		//  count useless writes within an element
	int16_t		offset;			//	difference between source->adrs and dst->adrs
	int8_t		oper;			//  operation of input
	uint8_t		*orig;			//	pointer to source orginal data
	int8_t		read;			//  'true' when oper is not one of the writes
	int16_t		segment;		//	segment of the memory transaction
	int32_t		size;			//	size of input transaction
	int64_t		src_adrs;		//	starting address of input
	uint8_t		src_data;		//  current cache line data byte
	int16_t		src_ndx;		//	index of byte into the source data array
	int8_t		src_valid;		//	indicate validity of source byte
	uint8_t		*stat;			//	pointer to array of status bytes for input
	int16_t		sub_dead;		//  count dead writes within current subblock
	int16_t		sub_dusty;		//  count dusty writes within current subblock
	int16_t		sub_live;		//  count live writes within current subblock
	int16_t		sub_read;		//  count reads within current subblock
	int16_t		sub_size;		//	bytes per subblock
	int16_t		sub_untouch;	//  count untouched bytes within current subblock
	int16_t		sub_usls;		//  count useless writes within current subblock
	int16_t		valid;			//	set if cache line is valid
	
	static
	uint8_t		mr_rdstat[MAX_MR_DATA] = {MRRDSTAT | CBLEMNT, MRRDSTAT, MRRDSTAT, MRRDSTAT,
		MRRDSTAT, MRRDSTAT, MRRDSTAT, MRRDSTAT, MRRDSTAT, MRRDSTAT, MRRDSTAT, MRRDSTAT,
		MRRDSTAT, MRRDSTAT, MRRDSTAT, MRRDSTAT};
	static
	uint8_t		mr_wrstat[MAX_MR_DATA] = {MRWRSTAT | CBLEMNT, MRWRSTAT, MRWRSTAT, MRWRSTAT,
		MRWRSTAT, MRWRSTAT, MRWRSTAT, MRWRSTAT, MRWRSTAT, MRWRSTAT, MRWRSTAT, MRWRSTAT,
		MRWRSTAT, MRWRSTAT, MRWRSTAT, MRWRSTAT};
	


#ifdef DEBUG_REF
	print_cntrs("entry", dst->owner, dst->segment);
#endif
	
	segment = dst->segment;
	cash = dst->owner;
	sub_size = cash->config->sb_siz;
	
	//	Determine if input is coming from mr or cl and set local variables accordingly
	if (mr) {
		if (cl) {
			error("Both sources of update_cl are valid", -32);
		}
		//	set local variables from mr input
		offset = (int16_t) mr->adrs - dst->adrs;
		data = &(mr->data[0]);
		orig = data;
		oper = mr->oper;
		size = mr->size;
		src_adrs = mr->adrs;
		if (oper == MRWRITE || oper == MRMODFY) {	//	select appropriate status tags
			if (mr->split) {
				stat = &mr_wrstat[1];	//	continuation, so do not include element tag byte
			} else {
				stat = &mr_wrstat[0];	//	not a continuation, so need element tag byte
			}
		} else {
			if (mr->split) {
				stat = &mr_rdstat[1];	//	continuation, so do not include element tag byte
			} else {
				stat = &mr_rdstat[0];	//	not a continuation, so need element tag byte
			}
		}
		valid = 1;
	} else {
		if (cl == NULL) {
			error("Both sources of update_cl are NULL", -33);
		}
		//	set local variables from cl input
		offset = (int16_t) cl->adrs - dst->adrs;
		data = &(cl->data[0]);
		orig = &(cl->orig[0]);
		oper = cl->oper;
		size = cl->owner->config->lin_siz;
		stat = cl->stat;
		src_adrs = cl->adrs;
		valid = cl->valid;
	}
	read = !(oper == MRWRITE  ||  oper == MRMODFY);
	

	//	initialize all flags
	blk_dead = 0;
	blk_dusty = 0;
	blk_live = 0;
	blk_untouch = 0;
	blk_usls = 0;
	lmt_flag = 0;
	sub_dead = 0;
	sub_dusty = 0;
	sub_live = 0;
	sub_read = 0;
	sub_untouch = 0;
	sub_usls = 0;
	
	//	Loop through all of the source bytes
	for (src_ndx = 0; src_ndx < size; src_ndx++) {
		dst_ndx = src_ndx + offset;
		src_data = data[src_ndx];
		dst_data = dst->data[dst_ndx];
		src_valid = (stat[src_ndx] & CBVALID);
		dst_valid = (dst->stat[dst_ndx] & CBVALID);
		
		//	if source indicates an element start, clear flags and set destination status correctly
		if (stat[src_ndx] & CBLEMNT) {
			lmt_dead = 0;
			lmt_dusty = 0;
			lmt_flag = 1;
			lmt_live = 0;
			lmt_untouch = 0;
			lmt_usls = 0;
			dst->stat[dst_ndx] |= CBLEMNT;		//	set element bit
		} else {
			dst->stat[dst_ndx] &= CBLMNTMSK;	//	clear element bit
		}
		
		//	update the appropriate byte resolution counter for this byte
		//	and set appropriate block, element, and sub-block flags
		if (read) {
			cash->cntrs[segment][BYT_RES][READ_ACS]++;			//	update read byte counter
			blk_read = 1;
			lmt_read = 1;
			sub_read = 1;
			dst->stat[dst_ndx] = (dst->stat[dst_ndx] & CBLSTMSK)  | CBLSTRD;		//	mark last access RD
		} else {
			if (src_valid == 0) {
				cash->cntrs[segment][BYT_RES][UNTOUCH]++;
				blk_untouch = 1;
				lmt_untouch = 1;
				sub_untouch = 1;
			} else if (cash->level != 1 &&  is_dead(src_adrs + src_ndx, segment) ) {	//  L1 never dead
				cash->cntrs[segment][BYT_RES][DEAD_WR]++;
				blk_dead = 1;
				lmt_dead = 1;
				sub_dead = 1;
			} else if ( src_valid == 1  &&  dst_valid == 1  &&  src_data == dst_data) {
				cash->cntrs[segment][BYT_RES][DUST_WR]++;
				blk_dusty = 1;
				lmt_dusty = 1;
				sub_dusty = 1;
			} else if (dst->stat[dst_ndx] & CBLSTWR) {
				cash->cntrs[segment][BYT_RES][USLS_WR]++;
				blk_usls = 1;
				lmt_usls = 1;
				sub_usls = 1;
			} else {
				cash->cntrs[segment][BYT_RES][LIVE_WR]++;
				blk_live = 1;
				lmt_live = 1;
				sub_live = 1;
			}
			dst->data[dst_ndx] = data[src_ndx];
			dst->stat[dst_ndx] = (dst->stat[dst_ndx] & CBLSTMSK)  | CBLSTWR;		//	mark last access WR
		}

		//	update orig field if destination not already valid and source is valid, then mark as valid
		if (dst_valid == 0  &&  src_valid) {
			dst->orig[dst_ndx] = orig[src_ndx];
			dst->stat[dst_ndx] = dst->stat[dst_ndx] | CBVALID;
		}
		dst->data[dst_ndx] = data[src_ndx];				//	copy current data
		
		//	update element counters if an element start was processed AND
		//	(this byte is the last byte of the access {src_ndx + 1 == size}
		//		OR  this byte is the last byte of the element (next byte is element start)
		//			{src_ndx + 1 < size  AND  stat[src_ndx+1] & CBLEMNT is true} )
		//	reset element start flag here, element count flags get reset at top of loop on next byte
		if (lmt_flag != 0  &&  ( src_ndx+1 == size  ||
			( (src_ndx + 1 < size)  &&  (stat[src_ndx+1] & CBLEMNT) ) ) ) {
			//	do not allow elements to be of mixed types
			if (lmt_read) {
				cash->cntrs[segment][LMT_RES][READ_ACS]++;
			} else if (lmt_dead) {
				cash->cntrs[segment][LMT_RES][DEAD_WR]++;
			} else if (lmt_live) {
				cash->cntrs[segment][LMT_RES][LIVE_WR]++;
			} else if (lmt_dusty) {
				cash->cntrs[segment][LMT_RES][DUST_WR]++;
			} else if (lmt_usls) {
				cash->cntrs[segment][LMT_RES][USLS_WR]++;
			} else if (lmt_untouch) {
				cash->cntrs[segment][LMT_RES][UNTOUCH]++;
			}
			lmt_flag = 0;
		}
		
		//	update sub-block counters if this byte is last byte of a sub-block
		//	use (src_adrs + src_ndx) in case mr->adrs starts in middle of sub-block
		//  reset flags here
		if ( ( (src_adrs + src_ndx) % sub_size == sub_size-1)		//	end of sub-block
			|| (src_ndx + 1 == size) ) {							//	or last byte of transaction
			if (sub_read) {
				cash->cntrs[segment][SUB_RES][READ_ACS]++;
			} else if (sub_dead + sub_dusty + sub_untouch >= 1)	{	//	mixed wasted write
				cash->cntrs[segment][SUB_RES][MIXD_WR]++;
			} else if (sub_dead == 1) {
				cash->cntrs[segment][SUB_RES][DEAD_WR]++;
			} else if (sub_dusty == 1) {
				cash->cntrs[segment][SUB_RES][DUST_WR]++;
			} else if (sub_live == 1) {
				cash->cntrs[segment][SUB_RES][LIVE_WR]++;
			} else if (sub_usls == 1) {
				cash->cntrs[segment][SUB_RES][USLS_WR]++;
			} else {
				cash->cntrs[segment][SUB_RES][UNTOUCH]++;
			}
			sub_dead = 0;
			sub_dusty = 0;
			sub_live = 0;
			sub_read = 0;
			sub_untouch = 0;
			sub_usls = 0;
		}
		
	}	//	end for of source bytes
	
	//	update block counters, no need to reset flags
	if (blk_read) {
		cash->cntrs[segment][BLK_RES][READ_ACS]++;
	} else if (blk_dead + blk_dusty + blk_untouch >= 1)	{	//	mixed wasted write
		cash->cntrs[segment][BLK_RES][MIXD_WR]++;
	} else if (blk_dead == 1) {
		cash->cntrs[segment][BLK_RES][DEAD_WR]++;
	} else if (blk_dusty == 1) {
		cash->cntrs[segment][BLK_RES][DUST_WR]++;
	} else if (blk_live == 1) {
		cash->cntrs[segment][BLK_RES][LIVE_WR]++;
	} else if (blk_usls == 1) {
		cash->cntrs[segment][BLK_RES][USLS_WR]++;
	} else {
		cash->cntrs[segment][BLK_RES][UNTOUCH]++;
	}
	//	update cache line status  TBD logic for subblocks needed
	dst->valid |= valid;
	if (oper == MRWRITE) {
		dst->dirty |= 1;
	} else {
		dst->referncd |= 1;
	}
	
		
#ifdef DEBUG_REF
	print_cntrs("exit", dst->owner, dst->segment);
#endif

}


//  There are segments of test code preserved below.  These can be copied back into
//  the indicated source area for detailed testing of low level functionality of moola

/*  Place the following after "set->access[segment]++;" as
 //	test code to verify move2_lru() and move2_mru()
 int32_t		nmbr_lines;		//	number of lines in the set
 int32_t		n;				//	loop index for the lines
 
 nmbr_lines = set->owner->assoc;
 cl = set->mru;
 for (n = 0; n < nmbr_lines; n++) {
 cl->adrs = tagadrs + n;
 cl = cl->lru;
 }
 print_set(set);
 cl = set->mru->lru->lru->lru;
 move2_lru(set, cl);
 print_set(set);
 move2_mru(set, cl);
 print_set(set);
 move2_mru(set, set->mru);
 print_set(set);
 move2_lru(set, set->mru);
 print_set(set);
 move2_mru(set, set->lru);
 print_set(set);
 move2_lru(set, set->lru);
 print_set(set);
 cl = set->mru->lru;
 move2_lru(set, cl);
 print_set(set);
 cl = set->mru->lru;
 move2_mru(set, cl);
 print_set(set);
 cl = set->lru->mru;
 move2_lru(set, cl);
 print_set(set);
 cl = set->lru->mru;
 move2_mru(set, cl);
 print_set(set);
 
 cl = set->lru->mru;
 
 //	end of move2_lru() and move2_mru() test code
 */

