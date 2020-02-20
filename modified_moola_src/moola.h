//
//  moola.h  (data structures include file of Moola Multicore Cache Simulator)
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
//  This is the main header file for the Moola multi-core cache simulator from UNT.
//
//  Cache terminology used in Moola:
//  Level 1 (L1) cache is closest to the processor and is also the highest level cache.
//  Going to a higher cache means a cache closer to processor.
//  The Last Level Cache (LLC) or lowest level cache is the cache closest to memory.
//	Going to a lower level cache means a the next cache level towards memory.
//
////////////////////////////////////////////////////////////////////////////////


#ifndef moola_moola_h
#define moola_moola_h

#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

////////////////////////////////////////////////////////////////////////////////
//  The following define various debug flags used throughout moola
////////////////////////////////////////////////////////////////////////////////

//#define DEBUG_HEAP		//	debug heap allocate/free operations
//#define DEBUG_QUEUE		//	debug adding/removing trace records from queues
//#define DEBUG_REF			//	debug reference basics
//#define DEBUG_STACK		//	debug stack top/limit operations


////////////////////////////////////////////////////////////////////////////////
//  The following #defines control compilation versions for data values and Gleipnir
////////////////////////////////////////////////////////////////////////////////

//  DATAVALS should be defined for applications where data values are required.
//			Most applications only use addresses and the data values are not required.
//			Significant memory is saved by compiling without data values.
//  GLEIPNIR should be set to get visibility into the source code cache behavior.
//			Some memory space is reduced if this compile setting is not used.
//	The default is to compile with both flags set.
#define DATAVALS
#define GLEIPNIR

#define FSIZE 256
#define VSIZE 256


////////////////////////////////////////////////////////////////////////////////
//  The following define maximum values for the various data structures
////////////////////////////////////////////////////////////////////////////////

//  the maximum bytes in a single memory reference
//  nominally the following would be 16, however, Valgrind produces some 19 byte
//	instructions just prior malloc and free, due to disassembly bug
//  This is not required if DATAVALS is not set
#define MAX_MR_DATA 32

//  the maximum number of processor cores allowed
#define MAX_PIDS 32

//	the maximum number of distributed blocks within a distributed cache array
#define MAX_DISTR_BLKS 64


////////////////////////////////////////////////////////////////////////////////
//  The following define enumerated values for various codes
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
//  The following data structures are defined in this file and used throughout
//	the moola program:
//		Cache Configuration		-  defines parameters for a cache
//		Memory Reference Data	-  transfers data between caches/memory
//		Cache Line Data			-  Implements a cache line
//		Cache Data				-  Implements a cache
//		Address Activity		-  Tracks activity by cache line address
//  Most functions require a combination of cache/cacheline/memref structure
//  pointers as their inputs.
//  all structures are defined from large elements to small elements to minimize
//  padding by the compiler
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
//  Cache Configuration Structure
////////////////////////////////////////////////////////////////////////////////
//  The cache configuration structure provides the values for all of the parameters
//  for a cache.  There will be configuration instances for L1I, L1D, L2, and L3
////////////////////////////////////////////////////////////////////////////////

//  TBD  this is not complete/optimized: need codes for replace/*_pol

typedef struct cache_cfg_rec {
	int32_t		size;		//	number of bytes in the cache, 0=> not used
	int16_t		lin_siz;	//	number of bytes per cache line
	int16_t		sb_siz;		//	number of sub-blocks in a cache line
	int16_t		access;		//	number of processor cycles for data access time
	int16_t		control;	//	number of processor cycles for control access time
	int16_t		assoc;		//	associativity of the cache, 0 => fully associative
	char		arch;		//	architecture of cache: (b - blocking, d - distributed, h - hit-under-miss)
	char		coherent;	//	coherency policy, n - none, m - MSI, e - MESI, o - MOSI
	char		pref_pol;	//	prefetch policy (n -off, a - always, m - miss, ...)
	char		replace;	//	replacement strategy (L - LRU, R - random)
	char		shared;		//	indicates if cache is private (P) or shared (S)
	char		walloc_pol;	//	write allocate policy (A - alloc, X - off)
	char		write_pol;	//	write policy (B - write-back, T - write-through)
	char		write_sets;	//	output sets statistics for the cache when 'T'
} cache_cfg;


////////////////////////////////////////////////////////////////////////////////
//  Memory Reference data structure
////////////////////////////////////////////////////////////////////////////////
//  The memory reference structure provides the information to represent activity
//  from the processor to the memory hierarchy.  These records are created from
//	the input file and placed in the queues for each processor.  They are extracted
//  by pseudo time order and processed by the cache.  As records are released by
//	the cache processing, they are placed on the memref_free list and reused by
//	the input routine.  New records are malloc'd when the memref_free list is empty.
////////////////////////////////////////////////////////////////////////////////

typedef struct memref_rec memref;
struct memref_rec {
	int64_t		adrs;		//  start address of the memory reference
	int64_t		time;		//	tracks time of this transfer (see explanation below)
	int64_t		linenmbr;	//	source file line number that initiated this memref (used for debugging)
	memref		*next;		//	pointer to next memref in queue or idle list
	memref		*prev;		//	pointer to previous memref in queue
	int32_t		size;		//	size of transaction; allows up to 4 GByte heap request, 0 => full cache flag
#ifdef DATAVALS
	uint8_t		data[MAX_MR_DATA];	//  memory reference data
#endif
	int8_t		oper;		//  operation, see definitions just below
	int8_t		segmnt;		//	memory segment 0-4: global, heap, instruction, stack, other
	int8_t		pid;		//	processor ID initiating this request
	int8_t		asid;		//	ASID of the thread running here
	int8_t		split;		//	set to 1 when second half of split transaction
#ifdef GLEIPNIR
	int64_t		virt_adrs;	//	virtual address
	int32_t		h_enum;		//	heap enumeration
	char		fname[128];	//	function name
	char		vname[128];	//	variable name
	char		scope[3];	//	scope info
#endif
};

//	The time field needs additional explanation as follows:
//	Initially, the time field will contain the insertion 'time' of the record onto the
//	processor queue from the input file or the time of a writeback initiation.  The time
//	will be updated to the "current simulation" time when it is extracted from the queue
//	for processing.  Access times and miss penalties may be added to the time as required.

//	definitions for segment types
#define GLOBAL 0
#define HEAP 1
#define INSTR 2
#define STACK 3
#define OTHER 4
//
//  basic memory and cache operation types
//  0 - read operation
//  1 - write operation
//  2 - read-modify-write operation
//  3 - instruction fetch
//	4 - prefetch read
//  5 - unused
//  6 - unused
//  7 - prefetch instruction
//  8 - allocate memory
//	9 - free memory
// 10 - stack pointer value
// 11 - cache line invalidate operation
// 12 - cache line clean operation
#define MRREAD  0
#define MRWRITE 1
#define MRMODFY 2
#define MRINSTR 3
#define PRREAD  4
#define PRINSTR 7
#define XALLOC  8
#define XFREE   9
#define XSTACK  10
#define XNULL   11
#define CLNVALD 12
#define CLCLEAN 13

//  prefetch flag
#define MRPREF  4

//  this extracts the basic operation (read/instruction) from prefetch flag

#define MRBASE(x)	((x) & (0xb))
#define IS_PREF(x)  ((x) & (MRPREF))


//	Structure to define the memrec queues used for input transactions to the
//	processors.  This allows the input trace files to have memory references in
//  batches of 50 to 1000 instructions, yet extract memory references on a per
//	instruction interleave.
typedef struct queue_rec {
	memref		*newest;	//	points to newest entry, add to queue here, NULL => empty
	memref		*oldest;	//	points to oldest entry, take from queue here, NULL => empty
	int32_t		count;		//	number of entries in the queue
} mr_queue;



//  incomplete type definition for cache to allow links from sets and lines back to the
//	cache to which they belong.  See complete type definition for struct cache_rec below.
typedef struct cache_rec	cache;



////////////////////////////////////////////////////////////////////////////////
//  Cache Line data structure
////////////////////////////////////////////////////////////////////////////////
//	The cacheline structure is the basic structure of the program.  It includes
//  an address tag, N current data fields, N original data fields, N status fields,
//  M (sub)block status fields, lru/mru set link fields, operation.
////////////////////////////////////////////////////////////////////////////////

typedef struct cline_rec	cacheline;
struct cline_rec {
	int64_t		adrs;		//  start address of the cacheline
	cacheline	*lru;		//	less recently used cacheline (for replacement algorithm)
	cacheline	*mru;		//	more recently used cacheline (for replacement algorithm)
	cache		*owner;		//	point to cache that owns this line
	int64_t		alloctime;	//	time of allocation (is last field to align data as
	int64_t		time;		//	time of last access to this line
	uint8_t		*data;		//  blocksize bytes of cacheline data
	uint8_t		*orig;		//  blocksize bytes of original data
	uint8_t		*stat;		//	blocksize entries of data byte status
	int16_t		valid;		//	valid bits for up to 16 subblocks
	int16_t		referncd;	//	referenced bits for up to 16 subblocks
	int16_t		dirty;		//	dirty bits for up to 16 subblocks
	int16_t		shared;		//	shared bits for up to 16 subblocks
	int16_t		exclusiv;	//	exclusive bits for up to 16 subblocks
	int8_t		oper;		//  operation, see definitions for memref
	int8_t		segment;	//	memory segment 0-4: global, heap, instruction, stack, other
};


//  Definition of bits in the cache line status byte
//  bit 0 - set when the byte is valid
//  bit 1 - set when the byte is dirty
//  bit 2 - set when the byte has been accessed
//  bit 3 - set when the byte was accessed last as a read
//  bit 4 - set when the byte was accessed last as a write
//  bit 5 - set when the byte is the first byte of an element
//  bit 6 - set when the byte is the last byte of an element
#define CBVALID 1
#define CBDIRTY 2
#define CBTOUCH 4
#define CBLSTRD 8
#define CBLSTWR 16
#define CBFEMNT 32
#define CBLEMNT 64
//	mask for last read, last write bits
#define CBLSTMSK 0xe7
//	mask for first in element, last in element
#define CBLMNTMSK 0x9f
#define MRRDSTAT CBVALID | CBTOUCH | CBLSTRD
#define MRWRSTAT CBVALID | CBDIRTY | CBTOUCH | CBLSTWR



////////////////////////////////////////////////////////////////////////////////
//  Cache set data structure
////////////////////////////////////////////////////////////////////////////////
//  The cache set structure holds the associative set of cachelines that map to
//	the set address and it maintains activity counters for the set.
////////////////////////////////////////////////////////////////////////////////

typedef struct cacheset_rec	cacheset;
struct cacheset_rec {
	cacheline	*lru;		//	least recently used cache line
	cacheline	*mru;		//	most recently used cache line
	cache		*owner;		//	point to cache that owns this set
	int64_t		access[5];	//	number of accesses to this set (G, H, S, I, O)
	int64_t		clean[5];	//	number of cleans of a line in this set (G, H, S, I, O)
	int64_t		evict[5];	//	number of evictions from this set (G, H, S, I, O)
	int64_t		fetch[5];	//	number of fetches of this set (G, H, S, I, O)
	int64_t		hits[5];	//	number of hits for this set (G, H, S, I, O)
	int64_t		misses[5];	//	number of misses for this set (G, H, S, I, O)
	int64_t		nvalid[5];	//	number of invalidates for this set (G, H, S, I, O)
	int64_t		wrback[5];	//	number of writebacks for this set (G, H, S, I, O)
};


////////////////////////////////////////////////////////////////////////////////
//  Cache array data structure
////////////////////////////////////////////////////////////////////////////////
//  The cache structure describes the current cache and provides an array of
//  cache-line sets and  includes counters for the various datum being collected
//  in the simulation
////////////////////////////////////////////////////////////////////////////////

//	typedef struct cache_rec cache;		declaration completion
//			incomplete declaration occurs prior to typedef cacheline
struct cache_rec {
	cache		*lower;				//	lower level cache (towards memory) from this cache (NULL => memory)
	char		name[8];			//  text name of this cache
	cacheset	*sets;				//	pointer to the cache sets data
	cache_cfg	*config;			//	pointer to the configuration record used by this cache
	int64_t		acss_time[MAX_DISTR_BLKS];	//	cache accessing until the time point
	int64_t		miss_time[MAX_DISTR_BLKS];	//	cache miss processing until indicated time (= acss if no miss)
	int64_t		miss_tag[MAX_DISTR_BLKS];	//	tag of cache line being filled from earlier miss
	int16_t		last_busy;			//	last busy time point for computing idle time
	int64_t		idle_time;			//	time duration that cache was idle
	int64_t		wait_time;			//	time duration that accesses had to wait due to cache busy
	int64_t		tagmask;			//	mask to get tag address bits from access address (-1 << log2blksize)
	int64_t		blkmiss[XALLOC];	//	subblock miss count for each access type, w/wo prefetch
									//  TBD  need to add coherency counters as well
									//	TBD  divide these by memory segment also???
	int64_t		cap_blk[XALLOC];	//	capacity subblock miss count for each access type, w/wo prefetch
	int64_t		cap_miss[XALLOC];	//	capacity miss count for each access type, w/wo prefetch
	int64_t		cmpl_blk[XALLOC];	//	compulsary subblock miss count for each access type, w/wo prefetch
	int64_t		cmpl_miss[XALLOC];	//	compulsary miss count for each access type, w/wo prefetch
	int64_t		conf_blk[XALLOC];	//	conflict subblock miss count for each access type, w/wo prefetch
	int64_t		conf_miss[XALLOC];	//	conflict miss count for each access type, w/wo prefetch
	int64_t		fetch[XALLOC];		//	fetch count for each access type, w/wo prefetch
	int64_t		miss[XALLOC];		//	miss count for each access type, w/wo prefetch
	int64_t		split_blk;			//	number of split line accessses
	int64_t		bytes_read;			//  total bytes read
	int64_t		bytes_write;		//	total bytes written
	int64_t		cntrs[5][4][7];		//  global/stack/heap/instr/other, byte/element/subblk/blk,
									//	read/untouched/live/usls/dust/dead/mixed
	int32_t		setmask;			//	mask to get set select address bits from access address
	int32_t		nmbr_lines;			//	number of lines in the cache (derived from other parms)
	int32_t		nmbr_sets;			//	number of sets in the cache (derived from other parms)
	int16_t		access;				//	number of processor clocks to access this cache
	int8_t		log2size;			//	log 2 of cache size in bytes
	int8_t		log2blksize;		//	log 2 of block size in bytes (shift distance for set mask to ndx)
	int8_t		log2sbsize;			//	log 2 of sub-block size in bytes
//	int8_t		setshift;			//	number of bit positions to shift masked bits to form set index
	int8_t		assoc;				//	associativity of cache (1-16), 0 => fully associative
	int8_t		actual_way;			//	actual way for dynamic cache flashing
	
	int8_t		level;				//	level of this cache; 1 is closest to processor
	int8_t		prefetch;			//	set to 1 to enable prefetching of next line
									//  TBD write_policy, write_alloc_policy here or in config->field?
									//	TBD function pointers for policies
									//	TBD how to bring in hash table for fully associative cache?
	int8_t          ins_or_data;                    // 0 means ins, 1 meand data
};
//	define indices for the counters
//	segment id used as first index
//	resolution used as second index
#define BYT_RES 0
#define LMT_RES 1
#define SUB_RES 2
#define BLK_RES 3
//	write type used as third index
#define READ_ACS 0
#define UNTOUCH 1
#define LIVE_WR 2
#define USLS_WR 3
#define DUST_WR 4
#define DEAD_WR 5
#define MIXD_WR 6



////////////////////////////////////////////////////////////////////////////////
//  Cache line activity data structure
////////////////////////////////////////////////////////////////////////////////
//  This structure keeps track of how often this cache line is
//  allocated and evicted at each cache and the number of bytes/elements
//  written back each time it was dirty.  The data is maintained in a large
//  hash table with a linked list to resolve hash collisions.  This structure
//  can get large and slow, so it can be disabled using the memstats=0 flag.
////////////////////////////////////////////////////////////////////////////////

typedef struct memstat_rec	memstat;
struct memstat_rec {
	memstat		*next;				//	points to next memstat with current hash
	int64_t		address;			//	actual address for this stat record
	int32_t		*l1_dirties;		//	numbers of time with n dirty bytes at L1
	int32_t		*l2_dirties;		//	numbers of time with n dirty bytes at L2
	int32_t		*l3_dirties;		//	numbers of time with n dirty bytes at L3
	int64_t		l1_cycles;			//	number of cycles in L1
	int64_t		l2_cycles;			//	number of cycles in L2
	int64_t		l3_cycles;			//	number of cycles in L3
	int32_t		l1_allocs;			//  number of times allocated at L1
	int32_t		l2_allocs;			//  number of times allocated at L2
	int32_t		l3_allocs;			//  number of times allocated at L3
};

//#define MEMSTAT_SIZE  4099
#define MEMSTAT_SIZE 16381
//#define MEMSTAT_SIZE 65537
#define MEMSTAT_HASH(x) ((x) % (MEMSTAT_SIZE))

////////////////////////////////////////////////////////////////////////////////
//  The address stat structure provides information on activity for each cache line address used
//  in the simulation.
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//  The address index structure provides a hash indexed set of linked lists for the cacheline
//  activity statistics.
////////////////////////////////////////////////////////////////////////////////
//The cache-line activity statistics is an optional measurement controlled by the adrs_stat flag.
////////////////////////////////////////////////////////////////////////////////

//  NOTES/QUESTIONS:
//  Include "infinite" caches to classify compulsory/capacity/conflict misses?
//  Establish partitioned caches, such as stack/heap/global  ---  can be done as separate splits
//    similar to instruction/data and then merge the stats into combined L1 stats



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//  Function Prototypes
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//	function protoypes are listed in alphabetical order with the file containing
//	the function implementation as a trailing comment
////////////////////////////////////////////////////////////////////////////////

void		clean_all(cache *cash, int initial_way, int final_way);	//	reference.c
void		cl_init(cacheline *cl);									//	reference.c
void		clr_bit(int8_t *aray, int16_t bit);						//	utils.c
int32_t		configure(int argc, char * argv[]);						//	configure.c
void		create_test_gzfile();									//	tracegz.c
void		defaults(void);											//	configure.c
void		error(char *, int);										//	utils.c
void		free_memref(memref *);									//	utils.c
int8_t		get_bit(int8_t *aray, int16_t bit);						//	utils.c
memref	   *get_memref();											//	utils.c
void		halloc(memref *);										//	utils.c
void		hfree(memref *);										//	utils.c
int32_t		initialize();											//	configure.c
int32_t		init_cache(cache *, cache_cfg *);						//	configure.c
char		*int64_to_str(int64_t val, char *str);					//	utils.c
void		invalidate_all(cache *cash);							//	reference.c
int8_t		is_dead(int64_t adrs, int8_t segment);					//	reference.c
void		move2_lru(cacheset *set, cacheline *cl);				//	reference.c
void		move2_mru(cacheset *set, cacheline *cl);				//	reference.c
void		print_cache(cache *cash);								//	utils.c
void		print_cntrs(char *msg, cache *cash, int sgmnt);			//	reference.c
void		print_mr(memref *);										//	utils.c
void		print_set(cacheset *set);								//	utils.c
void		print_set_stats(cache *cash);							//	utils.c
void		put_bit(int8_t val, int8_t *aray, int16_t bit);			//	utils.c
void		queue_add(int16_t pid, memref *mr);						//	utils.c
memref	   *queue_take(int16_t pid);								//	utils.c
int64_t		reference(cache *cash, memref *mr, cacheline *cl);		//	reference.c
memref	   *ref_split(cache *cash, memref *mr);						//	reference.c
cacheline  *search(cache *cash, int64_t cladrs, int32_t set);		//	reference.c
void		set_bit(int8_t *aray, int16_t bit);						//	utils.c
void		trace_close_gleipnir_gz(int16_t fil);					//	trace_gleipnir.c
int32_t		trace_open_gleipnir_gz(int16_t);						//	trace_gleipnir.c
int32_t		trace_read_gleipnir_gz(int16_t, memref *);				//	trace_gleipnir.c
int32_t		trace_reopen_gleipnir_gz(int16_t);						//	trace_gleipnir.c
void		trace_close_gleipnir_txt(int16_t fil);					//	trace_gleipnir.c
int32_t		trace_open_gleipnir_txt(int16_t);						//	trace_gleipnir.c
int32_t		trace_read_gleipnir_txt(int16_t, memref *);				//	trace_gleipnir.c
int32_t		trace_reopen_gleipnir_txt(int16_t);						//	trace_gleipnir.c
void		trace_close_moola_txt(int16_t fil);						//	trace_moola.c
int32_t		trace_open_moola_txt(int16_t);							//	trace_moola.c
int32_t		trace_read_moola_txt(int16_t, memref *);				//	trace_moola.c
int32_t		trace_reopen_moola_txt(int16_t);						//	trace_moola.c
void		trace_close_moola_gz(int16_t fil);						//	trace_moola.c
int32_t		trace_open_moola_gz(int16_t);							//	trace_moola.c
int32_t		trace_read_moola_gz(int16_t, memref *);					//	trace_moola.c
int32_t		trace_reopen_moola_gz(int16_t);							//	trace_moola.c
int32_t		trace_read_moola_valtxt(int16_t, memref *);				//	trace_moola.c
int32_t		trace_read_moola_valgz(int16_t, memref *);				//	trace_moola.c
void		trace_close_pin_txt(int16_t fil);						//	trace_moola.c
int32_t		trace_open_pin_txt(int16_t);							//	trace_pin.c
int32_t		trace_read_pin_txt(int16_t, memref *);					//	trace_pin.c
int32_t		trace_reopen_pin_txt(int16_t);							//	trace_pin.c
void		trace_close_pin_gz(int16_t fil);						//	trace_pin.c
int32_t		trace_open_pin_gz(int16_t);								//	trace_pin.c
int32_t		trace_read_pin_gz(int16_t, memref *);					//	trace_pin.c
int32_t		trace_reopen_pin_gz(int16_t);							//	trace_pin.c
int32_t		trace_read_pin_valtxt(int16_t, memref *);				//	trace_pin.c
int32_t		trace_read_pin_valgz(int16_t, memref *);				//	trace_pin.c
void		update_cl(memref *mr, cacheline *cl, cacheline *dst);	//	reference.c


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//  External Variables in alphabetical order
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//	The following external definitions allow the main data structures to be accessed
//	throughout the moola project.

extern  int64_t		asids[MAX_PIDS];		// ASIDS for use with -unicore
extern	int32_t		batch_instr_count;		//	number of instructions expected per interleaved batch
extern  char		*cfg_fil_name;			//	configuration output file name
extern	int16_t		comb_i_d;				//	combine i-cache d-cache statistics when non-zero
extern  char		*csv_fil_name;		 	//	csv output file name
extern  char            *csv_fil_name2;                  //      csv output file name
extern	int64_t		data_offs[MAX_PIDS];	//	data address offsets used with -unicore
extern	int16_t		data_only;				//	flag set when no instructions are traced
extern	int64_t		flush_rate;				//	flush caches after every flush_rate instructions
extern	memref		*free_mrs;				//	list of idle memref instances
extern	int64_t		instr_offs[MAX_PIDS];	//	instruction address offsets used with -unicore, -unicore_sh
extern	int16_t		in_filcnt;				//	number of input files to process
extern	char		*in_fnames[MAX_PIDS];	//	pointer to input file names
extern	char		*in_format;				//	input file format
extern	cache		l1d[MAX_PIDS];			//	array of Level 1 data caches (1 for each processor)
extern	cache_cfg	l1d_cfg;				//	configuration values for Level 1 data cache
extern	cache		l1i[MAX_PIDS];			//	array of Level 1 instr caches (1 for each processor)
extern	cache_cfg	l1i_cfg;				//	configuration values for Level 1 instruction cache
extern	cache		l2[MAX_PIDS];			//	array of Level 2 caches (1 for each processor if private)
extern	cache_cfg	l2_cfg;					//	configuration values for Level 2 unified cache
extern	cache		l3;						//	Level 3 cache
extern	cache_cfg	l3_cfg;					//	configuration values for Level 3 unified cache
extern	cache		l4;						//	Level 4 cache
extern	cache_cfg	l4_cfg;					//	configuration values for Level 4 unified cache
extern	cache		l5;						//	Level 5 cache
extern	cache_cfg	l5_cfg;					//	configuration values for Level 5 unified cache
extern	int32_t		max_mr_queue_size;		//	maximum mr_queue size before stopping input loop
extern	cache		mem;					//	memory "cache" for collecting statistics only
extern	cache_cfg	mem_cfg;				//	configuration values for memory pseudo cache
extern  FILE		*memtracefil;			//	file for optional memory trace output
extern	char		*mropers[13];			//	strings for printing memref operation codes
extern	int16_t		multiexpand;			//	set to read multiple files and expand each to multiple procs
extern	int16_t		nmbr_cores;				//	number of cores used in this run
extern	int16_t		output_sets;			//	causes set statistics to be output when set to 1
extern	mr_queue	queues[MAX_PIDS];		//	input queue for each processor
extern	char		*run_name;				//	name applied to this moola run
extern	char		*seg_code;				//	character codes for memory segment
extern	char		*sharemap[MAX_PIDS];	//	directs unicore file data to processors with shared I-adrs
extern	pid_t		sim_pid;				//	process id of the current Moola simulation run
extern	int64_t		sim_time;				//	simulated time
extern	int64_t		snapshot;				//	take snapshot every snapshots instructions, 0 off
extern	int64_t		stacklimit[MAX_PIDS];	//	limit of the stack growth for each processor
extern	int64_t		stacktop[MAX_PIDS];		//	current top of stack for each processor
extern	int64_t		stklmt_cnt[MAX_PIDS];	//	counts number of stack limit actions for each processor
extern	int64_t		stktop_cnt[MAX_PIDS];	//	counts number of stack actions for each processor
extern	int16_t		strict_order;			//	set to 1 if strict ordering is requested
extern	void		(*trace_close)(int16_t);			//	close function pointer for trace files
extern	int32_t		(*trace_open)(int16_t);				//	open function pointer for trace files
extern	int32_t		(*trace_read)(int16_t, memref *);	//	read function pointer for trace files
extern	int32_t		(*trace_reopen)(int16_t);			//	reopen function pointer for trace files
extern	int16_t		*unimap[MAX_PIDS][MAX_PIDS];		//	-unicore file to processors map
extern	int32_t		unidlys[MAX_PIDS];		//	-unicore replication delay between processor start times
extern	int16_t		unirepeats[MAX_PIDS];	//	-unicore files repeat counts
extern	char		*version;				//	version string

int             scheme;
uint64_t        cache_flash;
int 		cache_test;
uint64_t        counter_reflash;
uint64_t 		counter_dynamic;
int 			increase_way;
uint64_t 		dynamic_flash;
int 		initial_way;
int 		final_way;
int 		reset_DES;
uint64_t 	des_key;
uint64_t        counter;
uint64_t       max_addr,min_addr;
uint64_t 	a_number;
FILE          *csvf_2;                                //      point to csv format output file
FILE          *csvf_3;                                //      point to csv format output file
//int64_t         counter_a =0 ;


#define SIZE 100000
uint64_t v[SIZE];
uint64_t vector_index;
int set__lines;

// please dont forget add #include <inttypes.h>

struct sets{
   uint64_t elements;
   uint64_t num_1[42];
   double p1[42];
   double info;
   double set_index_info;
   double tag_info;
   // correlation part
   uint32_t sum_S[42][42];
   uint32_t sum_D[42][42];
   double info_bit[42];
   double corr_info[42];
   double c_total_info;
};

#define SETS 16384
#define SCHEMES 10
struct sets mysets[SETS];

#endif

