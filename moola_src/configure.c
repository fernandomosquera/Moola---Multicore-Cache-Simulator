//  configure.c  (configuration and initialization of the Moola Cache Simulation)
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
//  This is the configuration source file for the Moola multi-core cache simulator.
//	It processes the command line arguments in order until it finds the filename
//	for a configuration file and then gets parameters from that file.  It then
//	continues processing the command line arguments.  This allows a common configuration
//	file with a few parameters being overwritten by the command line.  An output
//	file is produced with the final configuration used in the current run.
//	The staticly defined configuration fields l1d_cfg, l1i_cfg, l2_cfg, and l3_cfg
//	are filled in during the configuration process.  The configuration is checked
//	for completeness and consistency.  0 is returned if the configuration completes
//	successfully, otherwise -1 is returned.
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
//#include <limits.h>			//	required by strtol by documentation, but Mac OS X did not need it?
#include <unistd.h>				//	needed for getpid()

#include "moola.h"

/*
extern	void		(*trace_close)(int16_t);
extern	int32_t		(*trace_open)(int16_t);
extern	int32_t		(*trace_read)(int16_t, memref *);
*/

void configure_ivybridge(void);

int16_t read_cfgfile(FILE *fptr, int16_t tkn, char *fnam);

char		*nxt_str;		//	start of next local argument string
char		strings[4000];	//	should be enough space for all option/value strings;
char		*tokens[500];	//	pointers for up to 500 tokens (250 option/value pairs)
int16_t		tkn_cnt;		//	count of tokens in tokens


//	TBD:  determine all configure return codes and document them

int32_t	configure(int argc, char * argv[]) {
	//	All of the help text is grouped here.  The general help, then alphabetically for each option
	char		*gen_hlp =
		"Moola is a multi-core cache simulator for evaluating cache effectiveness in\n"
		"a multiple core processor environment.  It is highly configurable and provides\n"
		"huge amounts of hit/miss, usage, and timing statistics at a variety of levels.\n"
		"Type '-ha' or '-help all' for a list of all options or '-h option_name' for help\n"
		"on a specific option.  Cache designation prefixes such as l1i_ or l2_ are not\n"
		"required when requesting help for a cache parameter such as 'access' or 'replace'.\n"
		"Moola was written by Charles Shelor at University of North Texas in 2013 and is\n"
		"provided for public use by the GPL license.\n\n"
		"Options available to Moola, their formats and (defaults) are the following:\n"
		"-asid          int_list    ASID for each thread for use with -unicore\n"
		"-{c}_arch      string      cache architecture: blocking | hum | distr\n"
		"-cfg           string      filename for configuration parameters (none)\n"
		"-cfg_out       string      filename for configuration report (moola_cfg_{pid}.txt)\n"
		"-comb_i_d      0 or 1      1 combines instruction and data cache statistics (0)\n"
		"-cores         int         number of cores to simulate (none)\n"
		"-csvfile       string      filename for comma separated values output file (none)\n"
		"-data_offset   int_list    data address offset amount for use with -unicore\n"
		"-data_only                 use this option when instructions are not traced\n"
		"-flush_rate    int         cache will be flushed after every int instructions (0)\n"
		"-h -help       option      print detailed help for the option or 'all' options\n"
		"-h -help                   print this message\n"
		"-ha                        print detailed help for all options\n"
		"-informat      string      select input format to use {moola | dinero_D | pin | ...\n"
		"-instr_offset  int_list    instruction address offset amount used with -unicore to share instrs\n"
		"-{c}_access    int         add int cycles for each {c} access\n"
		"-{c}_arch      string      cache architecture: blocking | hum | distr  (blocking)"
		"-{c}_assoc     int         associativity of {c}\n"
		"-{c}_lnsize    int         size of a {c} cache line in bytes\n"
		"-{c}_coherent  string      coherency protocol for {c} {none | MESI | MOSI} (none)\n"
		"-{c}_pref      string      prefetch policy for {c} {none | always | miss ...}\n"
		"-{c}_replace   string      line replacement policy for {c} {LRU | FIFO | RAND}\n"
		"-{c}_sbsize    int         size of a {c} sub block in bytes (lnsize)\n"
		"-{c}_share     char        'p' -> private cache, 's' -> shared cache, (L1/L2 -> p, L3/mem -> s)\n"
		"-{c}_size      int[sfx]    total size of {c} in bytes, [k] KB, or [m] MB\n"
		"-{c}_write     string      write policy for {c} {back | through}\n"
		"-mem_access    int         number of cycles to access main memory\n"
		"-memtrace      string      place trace of memory accesses in file 'string'\n"
		"-multicore     string      file name of multicore trace file\n"
		"-output_sets               output set statistics\n"
		"-preset        string      select a preset cache configuration {IvyBridge ...}\n"
		"-run_name      string      use string as the name for this moola run, default: 'moola_PID'\n"
		"-snapshot      int         generate snap shot output every int instructions\n"
		"-unicore       string int_list int  unicore trace file name applied to pn1,pn2,pn3 with int delay\n";
	char		*access_hlp =
		"The  '-C_access int'  option specifies the access time for the `C' cache, where `C' is one\n"
		"of `l1d', `l1i', `l2', `l3', or `mem'.  The `int' argument is required as a decimal integer\n"
		"representing cycles or some time amount.  The default values for `access' are 1 for l1i,\n"
		"0 for l1d, 5 for l2, 15 for l3, and 150 for mem.  See the documentation for how these are\n"
		"used for estimating the simulated time for a trace file 'execution'\n";
	char		*arch_hlp =
		"The '-C_arch string' option specifies the architecture of the `C' cache, where `C' is one\n"
		"of `l1d', `l1i', `l2', or `l3'.  The `string' argument is `blocking', `hum' for hit-under-miss\n"
		"or `distr' for distributed.  `blocking' will cause any accesses to the cache to wait for all\n"
		"prior cache activity to complete before starting the new access.  `hum' for hit-under-miss will\n"
		"cause the current access to wait for an access to the current level array to complete, but will\n"
		"proceed if there is lower level write-back or fill operations being performed.  A second miss\n"
		"waits for the first miss to be completed before it can be processed.  The `distr' option allows\n"
		"concurrent cache accesses if they are to separate blocks.  Accesses to the same block with a\n"
		"distributed cache follow the hit-under-miss timing model.\n";
	char		*assoc_hlp =
		"The  '-C_assoc int'  option specifies the associativity of the `C' cache, where `C' is one\n"
		"of `l1d', `l1i', `l2', or `l3'.  The `int' argument is required as a decimal integer and\n"
		"is the number of entries in each associative set.  A value of 0 makes the cache fully\n"
		"associative.  This value must be a power of 2.  There is no default, so every cache must\n"
		"have an `_assoc' value defined.\n";
	char		*cfg_hlp =
		"The  '-cfg filename'  option is used to read a text file containing Moola configuration\n"
		"options using the same format of the command line configuration options.  That is each\n"
		"option begins with a dash '-' followed by the option name, followed by an argument to the\n"
		"option if the option requires an argument.  Multiple options can be included on a line as\n"
		"long as they are separated by spaces.  Comments can be included and begin with a '#' and\n"
		"continue until the end of the line.  'filename' is a string of unquoted text giving the\n"
		"name of the file to read and must be provided as there is no default.  The -cfg option can\n"
		"be included in a file allowing nested configuration files.  All options are processed in the\n"
		"order encountered in the file so any option set multiple times will use the value that was\n"
		"read last.\n";
	char		*cfg_out_hlp =
		"The  '-cfg_out filename'  option is used to specify the file into which the configuration\n"
		"values for the simulation are written.  If this option is not used, the simulation configuration\n"
		"is written to 'moola_cfg_PID', where PID is the numerical value of the process id of the\n"
		"simulation process.  'filename' must be provided if the option is used as there is no default\n"
		"name for the option itself.  The file identified must be writable and it will be a text file\n"
		"with all of the options written such that it can be used in a subsequent '-cfg ' option.\n";
	char		*coherent_hlp =
		"The '-C_coherent string'  option specifies the coherency protocol to be used with cache C.  The\n"
		"allowed values for string are 'MESI', 'MOSI', and 'MSI'.  The default is MESI\n";
	char		*comb_i_d_hlp =
		"The default statistics reporting for split caches is for separate reports to be made.  Using\n"
		"the  '-comb_i_d'  option will cause the reports to be combined into a single report.  No\n"
		"argument is used with this option.\n";
	char		*cores_hlp =
		"The  'cores int'  option specifies the number of processor cores and private caches to include\n"
		"in the simulation.  Moola has been compiled for a maximum number of cores, usually 16, so trying\n"
		"to run the simulation with `int' set higher than this number will cause an error.  `int' should\n"
		"be a decimal number greater than 0 and less than or equal the maximum cores allowed.  There is\n"
		"no default value for 'cores' so it must be specified.\n";
	char		*csvfile_hlp =
		"The  '-cvsfile filename'  option specifies the output file to be written in comma separated\n"
		"values format suitable for importing into spreadsheet programs.  If this option is not used,\n"
		"a file named 'moola_out_PID.csv' is created in the local directory.  PID will be the numerical\n"
		"process id of the simulation when it was run.  Use '-cvsfile none' to prevent csv file output.\n";
	char		*data_offset_hlp =
		"The  '-data_offset int_list'  option is used in conjunction with the '-unicore' option for mapping\n"
		"a single trace file to multiple processors.  The int[i] value from the integer list of values is\n"
		"added to every data address that is accessed by processor i from the processor list in the\n"
		"'-unicore proc_list' option.  The int_list argument is a comma separated list of integers without\n"
		"spaces.  The integers can be decimal and must start with digits 1-9, hexadecimal and start with\n"
		"a '0x' prefix, or octal and start with a '0' digit.  The offsets should be set to avoid accidental\n"
		"data address sharing during the trace execution.  If multiple trace files are being read by using\n"
		"multiple '-unicore' options, then the data offsets should be set to ensure no addresses among the\n"
		"files overlap.  See also '-instr_offset int_list' and '-unicore int_list'\n";
	char		*data_only_hlp =
		"The  '-data_only'  option is used when there are no instructions in the trace record.  Initial\n"
		"transaction time is incremented for each transaction rather than for each instruction when this\n"
		"option is provided.  There is no argument to the option, default is to expect instructions.\n";
	char		*flush_rate_hlp =
		"The  '-flush_rate int'  option is used to simulate context switches.  The simulator will flush\n"
		"all of the caches every `int' instructions.  The value of `int' is interpreted as a decimal\n"
		"value.  The default value of '-flush_rate' is zero, which disables the periodic cache flushing.\n"
		"If '-flush_rate' is used, it does require a value for `int'.";
	char		*informat_hlp =
		"The  '-informat string'  option specifies the format of the trace records input file.  The\n"
		"default value is the standard Moola input format from a gzipped trace file, which is specified\n"
		"as `moola_gz'.  The complete list of supported formats is:  `dinero_d', `dinero_dgz', `dinero_D',\n"
		"`dinero_Dgz', `dinero_value', `dinero_valuegz', 'gleipnir', `moola', `moola_gz', `moola_value',\n"
		"`moola_valuegz', `pin', `pin_gz'.  See the documentation for details of these formats.\n"
		"Currently, only the 'moola' and 'gleipnir' formats support multi-core trace-files; the other\n"
		"formats can be used with the -unicore option.  The 'gleipnir' formats require compiling Moola\n"
		"with the GLEIPNIR flag defined.  The '_value' formats equire compiling Moola with the DATAVALS\n"
		"flag defined.  This compilation of Moola "
#ifdef DATAVALS
		"DOES support data values and "
#else
		"does NOT support data values and "
#endif
#ifdef GLEIPNIR
		"DOES support Gleipnir.\n";
#else
		"does NOT support Gleipnir.\n";
#endif
	char		*instr_offset_hlp =
		"The  '-instr_offset int_list'  option is used in conjunction with the '-unicore' option for mapping\n"
		"a single trace file to multiple processors.  The int[i] value from the integer list of values is\n"
		"added to every instruction address that is accessed by processor i from the processor list in the\n"
		"'-unicore proc_list' option.  The offsets should be set to avoid accidental instruction address\n"
		"sharing during the trace execution, or set to enable instruction address sharing for the processors.\n"
		"Sharing is enabled by setting the offsets to the same  See also '-data_offset' and '-unicore'.\n";
	char		*lnsize_hlp =
		"The  '-C_lnsize int'  option specifics the size of the cache line for the cache C.  The int value\n"
		"is a decimal number of bytes and should be a power of 2.  If can be specified as a hexadecimal\n"
		"number by prefixing the number with '0x', a leading 0 will indicate an octal number.  The default\n"
		"value is 64.\n";
	char        *memtrace_hlp =
		"The  '-memtrace string'  option specifies that a trace of memory accesses will be placed into the\n"
	    "file named 'string'.  The format is time: access_type address size.\n";
	char		*multicore_hlp =
		"The  '-multicore string'  option specifies the input file containing address trace records from\n"
		"a multicore program execution.  'string' is the name of the file containing the trace records.\n"
		"There is no default value for this option.  If neither -multicore nor -unicore is provided, there\n"
		"will be no trace records read.  Setting the string to the single character '-' will read stdin\n"
		"as the input file. Only one -multicore is valid, the last -multicore option encountered will be\n"
		"used for the simulation.  It is an error to mix -multicore and -unicore.  See also -unicore.\n";
	char		*output_sets_hlp =
		"The  '-output_sets'  option specifies that set statistics are to be output after processing the\n"
		"trace records.  There is no argument to this option.  The default is to not output set statistics.\n";
	char		*pref_hlp =
		"The '-C_pref string' option specifies the prefetch policy used for cache 'C'.  The allowed values\n"
		"for string are 'none', 'always', or 'miss'.  The default is 'none'.\n";
	char		*preset_hlp =
		"The  '-preset string'  option specifies a preset cache configuration.  Allowed preset values for\n"
		"'string' are 'IvyBridge4c8m' (4 core, 8 MB L3), 'A9' (Arm Cortex A9 2 core, 4 MB L2.  There is no\n"
		"default for the '-preset' option.  The specified preset cache can be modified by providing options\n"
		"after the -preset option.  For example, the sequence '-preset IvyBridge4c8m -l3_size 16M' will\n"
		"configure an IvyBridge 4 core system with a 16 megabyte L3 cache rather than the normal 8 MB.\n";
	char		*replace_hlp =
		"The '-C_replace string' option specifies the cache line replacement policy for cache 'C'.  The\n"
		"allowed values of string are 'LRU', 'FIFO', or 'RANDOM'.  The default value is 'LRU'.\n";
	char		*run_name_hlp =
		"The  '-run_name string'  option simply uses the 'string' value as a name for the Moola run.  If\n"
		"it is not specified, the default run_name is 'moola_PID', where PID is the numerical process ID\n"
		"of the moola execution process.  There can be no spaces in the string, '-' and '_' and other\n"
		"special symbols and non white-space characters are allowed.  The run_name string is used to\n"
		"identify the run in reports and as the default base name for output files.\n";
	char		*sbsize_hlp =
		"The  'C_sbsize int' option specifies the subblock size of the cache lines for cache 'C'.  The\n"
		"int argument gives the size of the sub block in bytes.  It should be a power of two and must be\n"
		"an integral divisor of the cache line size '-C_lnsize' value.  The default value is the value\n"
		"of the '-C_lnsize' option.  This results a cacheline and sub-block being equivalent.\n";
	char		*share_hlp =
		"The  'C_shared char'  option specifies whether the cache is shared or private.  The 'char'\n"
		"argument should be set to 'p' for private or 's' for shared.  The default is L1/L2 private,\n"
		"L3/mem shared.\n";
	char		*size_hlp =
		"The  'C_size int'  option specifies the total size of cache 'C' in bytes.  The int argument can\n"
		"be a decimal number by starting with digits 1-9, a hexadecimal number by starting with a 0x prefix\n"
		"or an octal value by starting with the digit 0.  An optional suffix of 'K' or 'M' can be used to\n"
		"specify the size in Kilobytes or Megaytes.  Thus specifying 32K is the same as 32768.  There can\n"
		"be no space between the number and the suffix.  There is no default value for this option and it\n"
		"must be specified for every cache that is used.  If a size is not specified, the cache is disabled.\n";
	char		*snapshot_hlp =
		"The  '-snapshot int'  option specifies that snapshot data will be output after every 'int'\n"
		"instructions.  'int' can be specified in octal (leading 0 digit), in decimal (leading 1-9 digit)\n"
		"or hexadecimal (leading 0x prefix).  The default value is 0 which turns snap shots off.\n";
	char		*unicore_hlp =
		"The  '-unicore string int_list int int' option specifies a unicore trace file as the input for this\n"
		"cache simulation.  The 'string' argument is the trace record file name.  The 'int_list' argument\n"
		"is a comma separated list of processor cores that will 'execute' the trace records.  The first\n"
		"'int' argument controls the starting times of the processors.  Each processor starts 'int' time\n"
		"units after the preceding processor starts; 'int' is >= 0.  The second 'int' argument is a\n"
		"repetition count that allows the input file to be read multiple times.  This allows short traces\n"
		"to be reloaded and run again to keep processors busy while longer traces are continuing to execute.\n"
		"There can be no spaces in 'int_list'.  See '-data_offsets' and '-instr_offsets' to control address\n"
		"space sharing when the '-unicore' trace file is replicated to multiple processors.  Multiple\n"
		"'-unicore' specifications can be provided to read multiple unicore trace files.  It is an error to\n"
		"map more than 1 file to the same processor core.  It is an error to mix -multicore and -unicore in\n"
		"the same moola execution.  Setting the string to the single character '-' will read stdin as the\n"
		"input file.  See also -multicore.\n";
	char		*write_hlp =
		"The  'C_write string'  option specifies the write policy used by cache 'C'.  The allowed values\n"
		"of 'string' are 'back' or 'through'.  The default value is 'back'.\n";

	
	int16_t		all_help;		//	set to 1 when large help requested, 0 otherwise
	cache_cfg	*cash_cfg;		//	pointer to cache configuration to update
	FILE		*cfgf;			//	pointer to configuration input file
	int16_t		cfg_error;		//	set when a configuration error occurs or help requested
	int16_t		chrcnt;			//	number of characters written into string
	int16_t		cla;			//	index for the command line argument loop
	char		*comma;			//	pointer to location of comma in option value list
	int16_t		help_prnt;		//	flag indicating that the requested option matched and printed
	char		last_char;		//	pointer to last character in size string to check for suffix
	int16_t		p;				//	loop index for possible processors in a file-to-processor mapping
	char		*tknptr;		//	pointer to current token string
	char		*tknbase;		//	pointer to base token part after any prefix stripped
	int16_t		token;			//	token index
	char		*valptr;		//	pointer to current token's value part
	char		*val;			//	pointer to current token's value part of interest
	
	for (token = 0; token < 500; token++) {
		tokens[token] = NULL;
	}
	nxt_str = strings;
	
	//	Set global configuration defaults
	chrcnt = sprintf(nxt_str, "moola_%d", sim_pid);
	run_name = nxt_str;
	nxt_str += chrcnt + 1;		//	the +1 keeps the '\0' string termination intact
	
	chrcnt = sprintf(nxt_str, "%s_cfg.txt", run_name);
	cfg_fil_name = nxt_str;
	nxt_str += chrcnt + 1;		//	the +1 keeps the '\0' string termination intact

	comb_i_d = 0;				//	default is to not combine the statistics for instruction and data
	
	chrcnt = sprintf(nxt_str, "%s.csv", run_name);
	csv_fil_name = nxt_str;
	nxt_str += chrcnt + 1;		//	the +1 keeps the '\0' string termination intact
	
	data_only = 0;
	flush_rate = 0;
	in_filcnt = 0;
	in_format = "moola_gz";
	max_mr_queue_size = 1000;	//	TBD this is not yet a configuration option and should be!
	multiexpand = -1;			//	establish as uninitialized
	nmbr_cores = 0;
	output_sets = 0;
	snapshot = 0;
	strict_order = 0;
	
	
	//  set cache defaults
	l1d_cfg.shared = 'P';
	l1i_cfg.shared = 'P';
	l2_cfg.shared  = 'P';
	l3_cfg.shared  = 'S';
	mem_cfg.shared = 'S';
	
	tkn_cnt = 0;
	cfg_error = 0;
	cfgf = fopen("moola_cfg.txt", "r");
	if (argc <= 1  &&  cfgf == NULL) {
		printf("No command line arguments were found and the default configuration file 'moola_cfg.txt'\n"
			   "could not be opened, please try again with appropriate Moola options and input data.\n");
		printf("%s", gen_hlp);
		return -2;
	} else {
		//	is there a default configuration file present
		if (cfgf) {
			token = 0;
			printf("Reading default configuration file:  'moola_cfg.txt'\n");
			read_cfgfile(cfgf, token, "moola_cfg.txt");
			fclose(cfgf);
		}
		//	copy the command line arguments, skip argv[0] as that is invocation name
		for (cla = 1; cla < argc; cla++) {
			tokens[tkn_cnt] = argv[cla];
			tkn_cnt++;
		}
	}
	
	//	Now process each token in the order it was seen, expanding configuration file reads
	//	as they are encountered.  Stop when no more tokens exist.
	//	First, extract cache designation prefix if it exists and set the cash_cfg pointer to
	//	the appropriate cache.  Then check options in alphabetical order, placing the help
	//	processing last.  (cache option parameter names do not have the '-' in the test string
	//	as those were removed as part of the cache id prefix.)
	token = 0;
	while (token < tkn_cnt) {
		tknptr = tokens[token++];						//	consume option token
		tknbase = tknptr;
		valptr = tokens[token++];						//	consume option value token (restore it if not used)
		
		cash_cfg = NULL;								//	early detect error if not set, but used
		if (strncmp(tknptr, "-l1d_", 5) == 0) {
			cash_cfg = &l1d_cfg;						//	point to l1d_cfg
			tknbase += 5;								//	skip past cache identifier
		} else if (strncmp(tknptr, "-l1i_", 5) == 0) {
			cash_cfg = &l1i_cfg;						//	point to l1d_cfg
			tknbase += 5;								//	skip past cache identifier
//		} else if (strncmp(tknptr, "-l1_", 4) == 0) {
//			cash_cfg = &l1_cfg;							//	point to l1_cfg
//			tknbase += 4;								//	skip past cache identifier
//		} else if (strncmp(tknptr, "-l2d_", 5) == 0) {
//			cash_cfg = &l2d_cfg;						//	point to l2d_cfg
//			tknbase += 5;								//	skip past cache identifier
//		} else if (strncmp(tknptr, "-l2i_", 5) == 0) {
//			cash_cfg = &l2i_cfg;						//	point to l2i_cfg
//			tknbase += 5;								//	skip past cache identifier
		} else if (strncmp(tknptr, "-l2_", 4) == 0) {
			cash_cfg = &l2_cfg;							//	point to l2_cfg
			tknbase += 4;								//	skip past cache identifier
		} else if (strncmp(tknptr, "-l3_", 4) == 0) {
			cash_cfg = &l3_cfg;							//	point to l3_cfg
			tknbase += 4;								//	skip past cache identifier
		} else if (strncmp(tknptr, "-l4_", 4) == 0) {
			cash_cfg = &l4_cfg;							//	point to l3_cfg
			tknbase += 4;								//	skip past cache identifier
		} else if (strncmp(tknptr, "-l5_", 4) == 0) {
			cash_cfg = &l5_cfg;							//	point to l3_cfg
			tknbase += 4;								//	skip past cache identifier
		} else if (strncmp(tknptr, "-mem_", 5) == 0) {
			cash_cfg = &mem_cfg;						//	point to mem_cfg
			tknbase += 5;								//	skip past cache identifier
		}
		
		if (strcmp(tknbase, "access") == 0) {
			cash_cfg->access = strtol(valptr, NULL, 0);
		} else if (strcmp(tknbase, "arch") == 0) {
			if (strcmp(valptr, "blocking") == 0) {
				cash_cfg->arch = 'b';
			} else if (strcmp(valptr, "hum") == 0) {
				cash_cfg->arch = 'h';
			} else if (strcmp(valptr, "distr") == 0) {
				cash_cfg->arch = 'd';
			} else {
				printf("Configuration error:  '%s' not a valid choice for %s\n", valptr, tknptr);
				cfg_error = 1;
				printf("%s\n", assoc_hlp);
			}
		} else if (strcmp(tknbase, "assoc") == 0) {
			cash_cfg->assoc = strtol(valptr, NULL, 0);
			if (cash_cfg->assoc & (cash_cfg->assoc -1) ) {
				printf("Configuration error:  %s value of %d is not a power of 2\n", tknptr, cash_cfg->assoc);
				cfg_error = 1;
				printf("%s\n", assoc_hlp);
			}
		} else if (strcmp(tknbase, "-cfg") == 0) {
			cfgf = fopen(valptr, "r");
			if (cfgf == NULL) {
				printf("Configuration error: unable to read file '%s'\n", valptr);
				cfg_error = 1;
			} else {
				read_cfgfile(cfgf, token, valptr);
			}
		} else if (strcmp(tknbase, "-cfg_out") == 0) {
			cfg_fil_name = valptr;
		} else if (strcmp(tknbase, "coherent") == 0) {
			if (strcmp(valptr, "none") == 0) {
				cash_cfg->coherent = 'n';
			} else if (strcmp(valptr, "MSI") == 0) {
				cash_cfg->coherent = 'm';
			} else if (strcmp(valptr, "MESI") == 0) {
				cash_cfg->coherent = 'e';
			} else if (strcmp(valptr, "MOSI") == 0) {
				cash_cfg->coherent = 'o';
			} else {
				printf("Configuration error:  '%s' not a valid choice for %s\n", valptr, tknptr);
				cfg_error = 1;
				printf("%s\n", coherent_hlp);
			}
		} else if (strcmp(tknbase, "-comb_i_d") == 0) {
			comb_i_d = 1;
			token--;									//	no value for this option, restore token index
		} else if (strcmp(tknbase, "-cores") == 0) {
			nmbr_cores = strtol(valptr, NULL, 0);
			if (nmbr_cores > MAX_PIDS) {
				printf("Configuration error: requested number of cores %d exceeds MAX_PIDS %d.  "
					   "change MAX_PIDS and recompile Moola\n", nmbr_cores, MAX_PIDS);
				cfg_error = 1;
			}
		} else if (strcmp(tknbase, "-csvfile") == 0) {
			csv_fil_name = valptr;
		} else if (strcmp(tknbase, "-data_offset") == 0) {
			for (p = 0; p < MAX_PIDS; p++) {
				data_offs[p] = strtol(valptr, &comma, 0);
				if (comma == NULL) {
					break;
				}
				valptr = comma + 1;
			}
			if (p != MAX_PIDS) {
				data_offs[p] = -1;				//	end of offsets
			}
		} else if (strcmp(tknbase, "-data_only") == 0) {
			data_only = 1;
			token--;									//	no value for this option, restore token index
		} else if (strcmp(tknbase, "-flush_rate") == 0) {
			flush_rate = strtol(valptr, NULL, 0);
		} else if (strcmp(tknbase, "-informat") == 0) {
			in_format = valptr;
			if (strcmp(valptr, "moola") == 0) {
				trace_close = trace_close_moola_txt;
				trace_open  = trace_open_moola_txt;
				trace_read  = trace_read_moola_txt;
				trace_reopen  = trace_reopen_moola_txt;
			} else if (strcmp(valptr, "moola_gz") == 0) {
				trace_close = trace_close_moola_gz;
				trace_open  = trace_open_moola_gz;
				trace_read  = trace_read_moola_gz;
				trace_reopen  = trace_reopen_moola_gz;
			} else if (strcmp(valptr, "moola_value") == 0) {
				trace_close = trace_close_moola_txt;
				trace_open  = trace_open_moola_txt;
				trace_read  = trace_read_moola_valtxt;
				trace_reopen  = trace_reopen_moola_txt;
			} else if (strcmp(valptr, "moola_valuegz") == 0) {
				trace_close = trace_close_moola_gz;
				trace_open  = trace_open_moola_gz;
				trace_read  = trace_read_moola_valgz;
				trace_reopen  = trace_reopen_moola_gz;
			} else if (strcmp(valptr, "pin") == 0) {
				trace_close = trace_close_pin_txt;
				trace_open  = trace_open_pin_txt;
				trace_read  = trace_read_pin_txt;
				trace_reopen  = trace_reopen_pin_txt;
			} else if (strcmp(valptr, "pin_gz") == 0) {
				trace_close = trace_close_pin_gz;
				trace_open  = trace_open_pin_gz;
				trace_read  = trace_read_pin_gz;
				trace_reopen  = trace_reopen_pin_gz;
			} else if (strcmp(valptr, "dinero_D") == 0) {
				printf("We're sorry, but this format is not yet implemented.\n");
				cfg_error = 1;
				trace_close = trace_close_moola_txt;
				trace_open  = trace_open_moola_txt;
				trace_read  = trace_read_moola_txt;
				trace_reopen  = trace_reopen_moola_txt;
			} else if (strcmp(valptr, "dinero_d") == 0) {
				printf("We're sorry, but this format is not yet implemented.\n");
				cfg_error = 1;
				trace_close = trace_close_moola_txt;
				trace_open  = trace_open_moola_txt;
				trace_read  = trace_read_moola_txt;
				trace_reopen  = trace_reopen_moola_txt;
			} else if (strcmp(valptr, "dinero_x") == 0) {
				printf("We're sorry, but this format is not yet implemented.\n");
				cfg_error = 1;
				trace_close = trace_close_moola_txt;
				trace_open  = trace_open_moola_txt;
				trace_read  = trace_read_moola_txt;
				trace_reopen  = trace_reopen_moola_txt;
			} else if (strcmp(valptr, "gleipnir") == 0) {
				trace_close = trace_close_gleipnir_txt;
				trace_open  = trace_open_gleipnir_txt;
				trace_read  = trace_read_gleipnir_txt;
				trace_reopen  = trace_reopen_gleipnir_txt;
			} else if (strcmp(valptr, "gleipnirgz") == 0) {
				trace_close = trace_close_gleipnir_gz;
				trace_open  = trace_open_gleipnir_gz;
				trace_read  = trace_read_gleipnir_gz;
				trace_reopen  = trace_reopen_gleipnir_gz;
			} else if (strcmp(valptr, "pixie32") == 0) {
				printf("We're sorry, but this format is not yet implemented.\n");
				cfg_error = 1;
				trace_close = trace_close_moola_txt;
				trace_open  = trace_open_moola_txt;
				trace_read  = trace_read_moola_txt;
				trace_reopen  = trace_reopen_moola_txt;
			} else if (strcmp(valptr, "pixie64") == 0) {
				printf("We're sorry, but this format is not yet implemented.\n");
				cfg_error = 1;
				trace_close = trace_close_moola_txt;
				trace_open  = trace_open_moola_txt;
				trace_read  = trace_read_moola_txt;
				trace_reopen  = trace_reopen_moola_txt;
			} else {
				printf("Configuration error:  '%s' not a valid choice for %s\n\n", valptr, tknptr);
				cfg_error = 1;
				printf("%s", informat_hlp);
			}
		} else if (strcmp(tknbase, "-instr_offset") == 0) {
			for (p = 0; p < MAX_PIDS; p++) {
				instr_offs[p] = strtol(val, &comma, 0);
				comma = strchr(valptr, ',');
				if (comma == NULL) {
					break;
				}
				val = comma + 1;
			}
			if (p != MAX_PIDS) {
				instr_offs[p] = -1;				//	end of offsets
			}
		} else if (strcmp(tknbase, "-asid") == 0) {
                        for (p = 0; p < MAX_PIDS; p++) {
                                int64_t dummy = strtol(valptr, &comma, 0);
                                if (comma == NULL) {
                                        break;
                                }
                                valptr = comma + 1;
                        }
                } else if (strcmp(tknbase, "lnsize") == 0) {
			cash_cfg->lin_siz = strtol(valptr, NULL, 0);
			if (cash_cfg->lin_siz & (cash_cfg->lin_siz -1) ) {
				printf("Configuration error:  %s value of %d is not a power of 2\n", tknptr, cash_cfg->lin_siz);
				cfg_error = 1;
				printf("%s\n", lnsize_hlp);
			}
		} else if (strcmp(tknbase, "-memtrace") == 0) {
			memtracefil = fopen(valptr, "w");
			if (memtracefil == NULL) {
				printf("Configuration error:  memory trace file '%s' could not be opened for writing\n",
					   valptr);
				cfg_error = 1;
				printf("%s\n", memtrace_hlp);
			}
		} else if (strcmp(tknbase, "-multicore") == 0) {
			in_fnames[0] = valptr;
			if (in_filcnt != 0) {
				printf("Configuration error:  only 1 %s can be specified and cannot be mixed with -unicore\n",
					   tknptr);
				cfg_error = 1;
				printf("%s\n", multicore_hlp);
			}
			in_filcnt = 1;
			multiexpand = 0;
		} else if (strcmp(tknbase, "-output_sets") == 0) {
			output_sets = 1;
			token--;									//	no value for this option, restore token index
		} else if (strcmp(tknbase, "pref") == 0) {
			if (strcmp(valptr, "none") == 0) {
				cash_cfg->pref_pol = 'n';
			} else if (strcmp(valptr, "always") == 0) {
				cash_cfg->pref_pol = 'a';
			} else if (strcmp(valptr, "miss") == 0) {
				cash_cfg->pref_pol = 'm';
			} else {
				printf("Configuration error:  %s not valid as a choice for %s\n", valptr, tknptr);
				cfg_error = 1;
				printf("%s\n", pref_hlp);
			}
		} else if (strcmp(tknbase, "-preset") == 0) {
			if (strcmp(valptr, "IvyBridge4c8M") == 0) {
				configure_ivybridge();
			} else if (strcmp(valptr, "Other") == 0) {
				//
			} else {
				printf("Configuration error:  %s not valid as a choice for %s\n", valptr, tknptr);
				cfg_error = 1;
				printf("%s\n", preset_hlp);
			}
		} else if (strcmp(tknbase, "replace") == 0) {
			if (strcmp(valptr, "LRU") == 0) {
				cash_cfg->pref_pol = 'L';
			} else if (strcmp(valptr, "rand") == 0) {
				cash_cfg->pref_pol = 'R';
			} else if (strcmp(valptr, "FIFO") == 0) {
				cash_cfg->pref_pol = 'F';
			} else {
				printf("Configuration error:  '%s' not valid as a choice for %s\n", valptr, tknptr);
				cfg_error = 1;
				printf("%s\n", replace_hlp);
			}
		} else if (strcmp(tknbase, "-run_name") == 0) {
			run_name = valptr;
		} else if (strcmp(tknbase, "sbsize") == 0) {
			cash_cfg->sb_siz = strtol(valptr, NULL, 0);
			if (cash_cfg->sb_siz & (cash_cfg->sb_siz -1) ) {
				printf("Configuration error:  %s value of %d is not a power of 2\n", tknptr, cash_cfg->sb_siz);
				cfg_error = 1;
				printf("%s\n", sbsize_hlp);
			}
		} else if (strcmp(tknbase, "share") == 0) {
			if (*valptr == 'p'  ||  *valptr == 'P') {
				cash_cfg->shared = 'P';
			} else if (*valptr == 's'  ||  *valptr == 'S') {
				cash_cfg->shared = 'S';
			} else {
				cfg_error = 1;
				printf("%s\n", share_hlp);
			}
		} else if (strcmp(tknbase, "size") == 0) {
			cash_cfg->size = (int32_t) strtol(valptr, NULL, 0);
			last_char = valptr[strlen(valptr) - 1];		//	get last character of the string
			if (last_char == 'k'  ||  last_char == 'K') {
				cash_cfg->size *= 1024;
			} else if (last_char == 'm'  ||  last_char == 'M') {
				cash_cfg->size *= 1048576;
			}
			if (cash_cfg->size & (cash_cfg->size -1) ) {
				printf("Configuration error:  %s value of %d is not a power of 2\n", tknptr, cash_cfg->size);
				cfg_error = 1;
				printf("%s\n", size_hlp);
			}
		} else if (strcmp(tknbase, "-snapshot") == 0) {
			snapshot = strtol(valptr, NULL, 0);
		} else if (strcmp(tknbase, "-unicore") == 0) {
			in_fnames[in_filcnt] = valptr;
			if (multiexpand == 0) {
				printf("Configuration error:  -unicore cannot be mixed with -multicore\n");
				cfg_error = 1;
				printf("%s\n", unicore_hlp);
			}
			multiexpand = 1;
			val = tokens[token++];				//	get third token for -unicore processor mapping
			for (p = 0; p < MAX_PIDS; p++) {
				unimap[in_filcnt][p] = strtol(val, &comma, 0);
				if (*comma == '\0') {
					break;
				}
				val = comma + 1;
			}
			if (p != MAX_PIDS) {
				unimap[in_filcnt][p+1] = -1;	//	indicate end of mapping for this file
			}
			val = tokens[token++];				//	get fourth token for -unicore start delays
			unidlys[in_filcnt] = (int32_t) strtol(val, NULL, 0);
			val = tokens[token++];				//	get fifth token for -unicore repeats
			unirepeats[in_filcnt] = (int16_t) strtol(val, NULL, 0);
			in_filcnt++;
		} else if (strcmp(tknbase, "write") == 0) {
			if (strcmp(valptr, "back") == 0) {
				cash_cfg->pref_pol = 'b';
			} else if (strcmp(valptr, "through") == 0) {
				cash_cfg->pref_pol = 't';
			} else {
				printf("Configuration error:  %s not valid as a choice for %s\n", valptr, tknptr);
				cfg_error = 1;
				printf("%s\n", write_hlp);
			}
//	template for additional options
//		} else if (strcmp(tknbase, "-option") == 0) {
		
			
			
			//  put the help processing here
		} else if (strcmp(tknbase, "-h") == 0  ||  strcmp(tknbase, "-ha") == 0
				   ||  strcmp(tknbase, "-help") == 0  ||  strcmp(tknbase, "--help") == 0) {
			all_help = 0;
			cfg_error = 2;				//	mark as help request, do not process
			if (strcmp(tknbase, "-ha") == 0) {
				all_help = 1;
			}
			if (valptr == NULL  &&  all_help == 0) {
				//	print short help
				printf("%s", gen_hlp);
				return 2;
			} else {
				help_prnt = 0;			//	mark as no help printed yet
				//	determine which option is being requested if it was not already 'all'
				if (all_help == 0) {
					if (strcmp(valptr, "all") == 0) {
						all_help = 1;
					} else {
						if (*valptr == '-') {
							valptr++;				//	skip past '-' if included in option name
						}
						if (*valptr == '_') {
							valptr++;				//	skip past '_' if included in option name
						}
						if (strncmp(valptr, "l1d_", 4) == 0) {
							valptr += 4;			//	skip past cache identifier
						} else if (strncmp(valptr, "l1i_", 4) == 0) {
							valptr += 4;			//	skip past cache identifier
						} else if (strncmp(valptr, "l2_", 3) == 0) {
							valptr += 3;			//	skip past cache identifier
						} else if (strncmp(valptr, "l3_", 3) == 0) {
							valptr += 3;			//	skip past cache identifier
						} else if (strncmp(valptr, "mem_", 4) == 0) {
							valptr += 4;			//	skip past cache identifier
						}
					}
				}
				if (all_help) {
					printf("%s\n", gen_hlp);
				}
				//	now check for 'all' or individual options
				if (all_help  ||  strcmp(valptr, "access") == 0) {
					printf("%s\n", access_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "arch") == 0) {
					printf("%s\n", arch_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "assoc") == 0) {
					printf("%s\n", assoc_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "cfg") == 0) {
					printf("%s\n", cfg_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "cfg_out") == 0) {
					printf("%s\n", cfg_out_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "coherent") == 0) {
					printf("%s\n", coherent_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "comb_i_d") == 0) {
					printf("%s\n", comb_i_d_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "cores") == 0) {
					printf("%s", cores_hlp);
					printf("The maximum number of cores for this compilation of Moola is %d\n\n", MAX_PIDS);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "csvfile") == 0) {
					printf("%s\n", csvfile_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "data_offset") == 0) {
					printf("%s\n", data_offset_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "data_only") == 0) {
					printf("%s\n", data_only_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "flush_rate") == 0) {
					printf("%s\n", flush_rate_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "informat") == 0) {
					printf("%s\n", informat_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "instr_offset") == 0) {
					printf("%s\n", instr_offset_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "lnsize") == 0) {
					printf("%s\n", lnsize_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "memtrace") == 0) {
					printf("%s\n", memtrace_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "multicore") == 0) {
					printf("%s\n", multicore_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "output_sets") == 0) {
					printf("%s\n", output_sets_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "pref") == 0) {
					printf("%s\n", pref_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "preset") == 0) {
					printf("%s\n", preset_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "replace") == 0) {
					printf("%s\n", replace_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "run_name") == 0) {
					printf("%s\n", run_name_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "sbsize") == 0) {
					printf("%s\n", sbsize_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "share") == 0) {
					printf("%s\n", share_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "size") == 0) {
					printf("%s\n", size_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "snapshot") == 0) {
					printf("%s\n", snapshot_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "unicore") == 0) {
					printf("%s\n", unicore_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (all_help  ||  strcmp(valptr, "write") == 0) {
					printf("%s\n", write_hlp);
					help_prnt = 1;					//	help option match was found
				}
				if (help_prnt == 0) {
					printf("Could not locate help on `%s'\n", tokens[token - 1]);
				}
				return 1;
			}
		} else {
			printf("ERROR: unknown option '%s' encountered\n", tknptr);
			return -1;
		}
	}
	
	
	//  Verify the configuration data is complete
	//  first check global items
	if (multiexpand == -1) {
		printf("ERROR: either 1 '-multicore' or 1 or more '-unicore' options must be specified\n");
		printf("%s\n", multicore_hlp);
		printf("%s\n", unicore_hlp);
		return -1;
	}
	
	
	//	Print the final configuration data into a file and to stdout
	
	
	
	return 0;
}

char		default_cfname[100];

						 
void defaults(void) {
	
	cfg_fil_name = sprintf(default_cfname, "moola_%d_cfg.txt", sim_pid);
}



void configure_ivybridge() {

	nmbr_cores = 4;
//  These are defined in the order of the structure definition to ensure all are set
	l1d_cfg.size = 32768;
	l1d_cfg.lin_siz = 64;
	l1d_cfg.sb_siz = 64;
	l1d_cfg.access = 0;
	l1d_cfg.control = 1;
	l1d_cfg.assoc = 8;
	l1d_cfg.pref_pol = 'X';		//	prefetch is off
	l1d_cfg.replace = 'L';		//	LRU replacement
	l1d_cfg.shared = 'P';		//	private for each processor
	l1d_cfg.walloc_pol = 'X';	//	write allocate is off
	l1d_cfg.write_pol = 'B';	//	write back is enabled
	
	l1i_cfg.size = 32768;
	l1i_cfg.lin_siz = 64;
	l1i_cfg.sb_siz = 64;
	l1i_cfg.access = 1;
	l1i_cfg.control = 1;
	l1i_cfg.assoc = 8;
	l1i_cfg.pref_pol = 'X';		//	prefetch is off
	l1i_cfg.replace = 'L';		//	LRU replacement
	l1i_cfg.shared = 'P';		//	private for each processor
	l1i_cfg.walloc_pol = 'X';	//	write allocate is off
	l1i_cfg.write_pol = 'B';	//	write back is enabled
	
	l2_cfg.size = 262144;
	l2_cfg.lin_siz = 64;
	l2_cfg.sb_siz = 64;
	l2_cfg.access = 12;
	l2_cfg.control = 4;
	l2_cfg.assoc = 8;
	l2_cfg.pref_pol = 'X';		//	prefetch is off
	l2_cfg.replace = 'L';		//	LRU replacement
	l2_cfg.shared = 'P';		//	private for each processor
	l2_cfg.walloc_pol = 'X';	//	write allocate is off
	l2_cfg.write_pol = 'B';		//	write back is enabled
	
	l3_cfg.size = 8388608;
	l3_cfg.lin_siz = 64;
	l3_cfg.sb_siz = 64;
	l3_cfg.access = 26;
	l3_cfg.control = 6;
	l3_cfg.assoc = 16;
	l3_cfg.pref_pol = 'X';		//	prefetch is off
	l3_cfg.replace = 'L';		//	LRU replacement
	l3_cfg.shared = 'S';		//	shared among all processors
	l3_cfg.walloc_pol = 'X';	//	write allocate is off
	l3_cfg.write_pol = 'B';		//	write back is enabled
	
	mem_cfg.size = 8388608;		//	most of the config settings are ignored for main memory
	mem_cfg.lin_siz = 64;
	mem_cfg.sb_siz = 64;
	mem_cfg.access = 150;
	mem_cfg.control = 30;
	mem_cfg.assoc = 16;
	mem_cfg.pref_pol = 'X';		//	prefetch is off
	mem_cfg.replace = 'L';		//	LRU replacement
	mem_cfg.shared = 'S';		//	shared among all processors
	mem_cfg.walloc_pol = 'X';	//	write allocate is off
	mem_cfg.write_pol = 'B';	//	write back is enabled

	return;
}

int32_t	initialize() {
	
	int8_t		pndx;		//	index for loop of processors
	int32_t		stat;		//	status from function calls
	
	//	Initialize the Level 1 Data Caches
	for (pndx = 0; pndx < nmbr_cores; pndx++) {
		sprintf(l1d[pndx].name, "L1D[%d]", pndx);
		l1d[pndx].level = 1;
		if (l2_cfg.shared == 'P') {
			l1d[pndx].lower = &l2[pndx];
		} else {
			l1d[pndx].lower = &l2[0];
		}
		stat = init_cache(&l1d[pndx], &l1d_cfg);
		if (stat) {
			return stat;
		}
	}
	
	//	Initialize the Level 1 Instruction Caches
	for (pndx = 0; pndx < nmbr_cores; pndx++) {
		sprintf(l1i[pndx].name, "L1I[%d]", pndx);
		l1i[pndx].level = 1;
		if (l2_cfg.shared == 'P') {
			l1i[pndx].lower = &l2[pndx];
		} else {
			l1i[pndx].lower = &l2[0];
		}
		stat = init_cache(&l1i[pndx], &l1i_cfg);
		if (stat) {
			return stat;
		}
	}
	
	//	Initialize the Level 2 Cache(s)
	if (l2_cfg.shared == 'P') {
		for (pndx = 0; pndx < nmbr_cores; pndx++) {
			sprintf(l2[pndx].name, "L2[%d]", pndx);
			l2[pndx].level = 2;
			l2[pndx].lower = &l3;
			stat = init_cache(&l2[pndx], &l2_cfg);
			if (stat) {
				return stat;
			}
		}
	} else {
		sprintf(l2[0].name, "L2");
		l2[0].level = 2;
		l2[0].lower = &l3;
		stat = init_cache(&l2[0], &l2_cfg);
		if (stat) {
			return stat;
		}
	}
	
	//	Initialize the Level 3 Cache
	sprintf(l3.name, "L3");
	l3.level = 3;
	l3.lower = &mem;
	stat = init_cache(&l3, &l3_cfg);
	if (stat) {
		return stat;
	}
	
	//	Initialize the Memory pseudo cache
	sprintf(mem.name, "MEM");
	mem.level = 4;								//	TBD:  always 4? what if no L3
	mem.lower = NULL;
	stat = init_cache(&mem, &mem_cfg);
	if (stat) {
		return stat;
	}
	
	return 0;
}


//  This function intializes the cache pointed to by *cash according to the
//  configuration pointed to by *ccfg.
int32_t	init_cache(cache *cash, cache_cfg *ccfg) {
	
	uint8_t		*data;		//	storage for data, orig, stat bytes
	uint8_t		*dptr;		//	data pointer into data array
	int32_t		lcnt;		//	line count index for set processing
	cacheline	*lines;		//	pointer for the line storage
	cacheline	*lptr;		//	line pointer into lines array
	cacheset	*sets;		//	pointer for the sets of the cache
	int32_t		sndx;		//	index for loop of sets of a cache

	//	setup configuration pointer, compute number of lines and sets, verify cache size
	cash->config = ccfg;
	cash->nmbr_lines = ccfg->size / ccfg->lin_siz;
	cash->nmbr_sets  = cash->nmbr_lines / ccfg->assoc;
	
	if (cash->nmbr_sets * ccfg->assoc * ccfg->lin_siz != ccfg->size) {
		fprintf(stderr, "Initialization error: sets %d * association %d * block size %d\n",
				cash->nmbr_sets, ccfg->assoc, ccfg->lin_siz);
		fprintf(stderr, "   did not equal cache size %d for cache %s\n",
				ccfg->size, cash->name);
		return -1;
	}
	cash->log2blksize = log2(ccfg->lin_siz);
	cash->log2size = log2(ccfg->size);
	cash->log2sbsize = log2(ccfg->sb_siz);
	
	
	//	Get the space for the lines and data/orig/stat storage
	lines = calloc(cash->nmbr_lines, sizeof(cacheline));
	if (lines == NULL) {
		fprintf(stderr, "ERROR, could not get %lld bytes of memory for lines of %s cache.\n",
				(int64_t) cash->nmbr_lines * (int64_t) sizeof(cacheline), cash->name);
		return -1;
	}
	data = calloc(cash->nmbr_lines, 3 * ccfg->lin_siz);
	if (data == NULL) {
		fprintf(stderr, "ERROR, could not get %lld bytes of memory for data/orig/stat of %s cache.\n",
				(int64_t) cash->nmbr_lines * 3 * (int64_t) ccfg->lin_siz, cash->name);
		return -2;
	}

	
	//	Get the space for the sets and set the pointers to the appropriate lines
	//	set the pointers to the data, orig, and stat areas for each line
	sets = calloc(cash->nmbr_sets, sizeof(cacheset));
	if (sets == NULL) {
		fprintf(stderr, "ERROR, could not get %ld bytes of memory for sets of %s cache.\n",
				cash->nmbr_sets * sizeof(cacheset), cash->name);
		return -3;
	}
	cash->sets = sets;		//	link the cache to the sets
	
	lptr = lines;
	dptr = data;
	for (sndx = 0; sndx < cash->nmbr_sets; sndx++) {
		//	processing for a single set of lines
		sets[sndx].mru = lptr;
		sets[sndx].lru = lptr + (ccfg->assoc - 1);
		sets[sndx].owner = cash;
		for (lcnt = 0; lcnt < ccfg->assoc; lcnt++) {
			lptr->data = dptr;
			lptr->orig = lptr->data + ccfg->lin_siz;
			lptr->stat = lptr->orig + ccfg->lin_siz;
			lptr->owner = cash;
			if (lcnt == 0) {
				//	this is MRU so its mru link must wrap to last in set
				lptr->mru = sets[sndx].lru;
				lptr->lru = lptr + 1;
			} else if (lcnt == ccfg->assoc - 1) {
				//	this is LRU so its lru link must wrap to first in set
				lptr->mru = lptr - 1;
				lptr->lru = sets[sndx].mru;
			} else {
				lptr->mru = lptr - 1;
				lptr->lru = lptr + 1;
			}
			//  other values of line init to 0 by calloc()
			//	values of data, orig, stat also init to 0 by calloc()
			lptr++;
			dptr += 3 * ccfg->lin_siz;
		}
		//	counters for the set are set to 0 by use of calloc() function.
	}

	
	cash->setmask = ((1 << (int8_t) log2(cash->nmbr_sets)) - 1) << cash->log2blksize;
	cash->tagmask = 0xffffffffffffffff << cash->log2blksize;
	cash->assoc = ccfg->assoc;
	cash->prefetch = ccfg->pref_pol;
	int32_t		i;
	for (i = 0; i < MAX_DISTR_BLKS; i++) {
		cash->acss_time[i] = 0;
		cash->miss_time[i] = 0;
		cash->miss_tag[i] = 0;
	}
	cash->idle_time = 0;
	cash->wait_time = 0;
	
	//	set all counters to 0
	for (i = 0; i < XALLOC; i++) {
		cash->blkmiss[i] = 0;
		cash->cap_blk[i] = 0;
		cash->cap_miss[i] = 0;
		cash->cmpl_blk[i] = 0;
		cash->cmpl_miss[i] = 0;
		cash->conf_blk[i] = 0;
		cash->conf_miss[i] = 0;
		cash->fetch[i] = 0;
		cash->miss[i] = 0;
	}
	cash->split_blk = 0;
	//cach->split_block[0] = cach->split_block[1] = cach->split_block[2] = cach->split_block[3] = 0;
	cash->bytes_read = 0;
	cash->bytes_write = 0;
	int64_t  *array = &cash->cntrs[0][0][0];
	for (i = 0; i < 5 * 4 * 7; i++)
		array = 0;
	
	//	need verification section here to make sure all needed parameters are set
	//	and dependent parameters have correct dependencies
	
	return 0;
}



//char		*nxt_str;		//	start of next local argument string
//char		strings[4000];	//	should be enough space for all option/value strings;
//char		*tokens[500];	//	pointers for up to 500 tokens (250 option/value pairs)/
int16_t read_cfgfile(FILE *fptr, int16_t tkn, char *fnam) {
	char		fbfr[250];		//	input file line buffer
	char		*sptr;			//	returned pointer from fgets
	char		*tkn1;			//	indicates start of a token
	char		*ftokens[500];	//	pointers for up to 500 tokens (250 option/value pairs)
	int16_t		ftkn;			//	index of next token in ftokens
	char		*ch;			//	character pointer for copying token string
	int16_t		tk;				//	token index for shifting existing
	
	ftkn = 0;
	while (1) {
		sptr = fgets(fbfr, 250, fptr);
		if (sptr == NULL) {		//	attempt to read past end of file
			break;
		}
		while (1) {
			if (*sptr == ' ' ||  *sptr == '\t' ||  *sptr == '=') {
				sptr++;
				continue;		//	skip leading white space or 'floating' equal sign
			}
			if (*sptr == '#'  ||  *sptr == '\0'  ||  *sptr == '\n') {
				break;			//	skip remainder of line as comment or reached end of line
			}
			//	This must be start of a token
			tkn1 = sptr;
			sptr++;				//	advance to next character
			while (1) {			//	find end of token
				if (*sptr == ' '  ||  *sptr == '\t'  ||  *sptr == '#'  ||  *sptr == '\0'
					||  *sptr == '\n'  ||  *sptr == '=') {
					break;		//	end of token found
				}
				sptr++;
			}
			
			//	set token pointer to current string and advance ftkn
			ftokens[ftkn++] = nxt_str;
			//	now copy token to the strings storage (sptr pointing 1 past last char of token)
			for (ch = tkn1; ch < sptr; ch++) {
				*nxt_str++ = *ch;	//	copy character
			}
			*nxt_str++ = '\0';		//	terminate the new string
		}
	}
	printf("There were %d tokens read from the configuration file %s\n", ftkn, fnam);
	
	//	now insert the tokens into the tokens array
	//	tkn indicates where to start placing these tokens, if tkn != tkn_cnt then move existing
	//  (This occurs when a configuration file specification is encountered and processed)
	//	then the tokens must be moved
	
	if (tkn_cnt != tkn) {
		//  need to move tkn through tkn_cnt-1 to tkn+ftkn to tkn_cnt-1+ftkn
		//  and MUST do this in reverse order to prevent potentially overwriting source tokens
		for (tk = tkn_cnt-1; tk >= tkn; tk--) {
			tokens[tk + ftkn] = tokens[tk];
		}
	}
	//	now copy the new tokens from ftokens to tokens
	for (tk = 0; tk < ftkn; tk++) {
		tokens[tkn + tk] = ftokens[tk];
	}
	//	update token count
	tkn_cnt += ftkn;
	return 0;
}



/*
 int64_t		a1, a2, a3, a4, a5;						//	for debugging visibility
 for (lndx = 0; lndx < cach->nmbr_lines; lndx++) {
 a1 = lines[lndx].data = (uint8_t *) &lines[lndx].alloctime + 8;
 a2 = lines[lndx].orig = (uint8_t *) lines[lndx].data + ccfg->lin_siz;
 a3 = lines[lndx].stat = (uint8_t *) lines[lndx].orig + ccfg->lin_siz;
 a4 = &lines[lndx].adrs;
 a5 = &lines[lndx].alloctime;
 lines[lndx].owner = cach;
 //  set the mru link to the next line and wrap to beginning of set at end
 if ((lndx + 1) % ccfg->assoc == 0) {
 lines[lndx].mru = &lines[lndx + 1 - ccfg->assoc];	//	last line of set, pt to 1st of set
 } else {
 lines[lndx].mru = &lines[lndx + 1];
 }
 //	set the lru link to the previous line wrapping to end of set initially
 if (lndx % ccfg->assoc == 0) {
 lines[lndx].lru = &lines[lndx + ccfg->assoc - 1];
 } else {
 lines[lndx].lru = &lines[lndx - 1];
 }
 //	adrs, data/orig/stat bytes, sub-block stats, and counters are set to 0
 //  by use of the calloc() function
 }
 */



