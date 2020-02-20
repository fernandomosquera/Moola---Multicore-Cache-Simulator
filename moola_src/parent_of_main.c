/*
int old(int argc, const char * argv[]) {
	char		*benches1[12] = {"adpcm_c", "adpcm_d", "charcnt", "dijkstra", "fft", "gobmk",
				"jpeg_c", "jpeg_d", "qsort_int", "stringsearch", "susan_c", "susan_e"};
	char		*benches2[13] = {"basicmath", "bitcount", "blowfish", "CRC32", "ffti", "gcc_200",
				"gcc_typeck", "gsm_toast", "gsm_untoast", "patricia", "qsort_text", "sha", "susan_s"};
	char		*benches3[1] = {"gcc_166"};
	char		*benches4[1] = {"bzip2"};
	char		*benches5[1] = {"mcf"};
	char		*benches6[1] = {"hmmr"};
	//  
	//  
	//   
	int16_t		bench;
	int16_t		select;
	int16_t		pr;
	int16_t		stat;
	
	if (argc < 2) {
		printf("Please select a benchmark group 1-6\n");
		select = 1;
	} else {
		select = atoi(argv[1]);
	}
	//sprintf(csvfname, "moola_%d.csv", select);
	sprintf(csvfname, 
			"/Users/charles/Documents/UNT/_Spring_2013/CSCE_5610_Architecture/assignments/project/moola_%d.csv",
			select);
	csvf = fopen(csvfname, "a");
	if (csvf == NULL) {
		printf("ERROR:  could not open '%s' for writing\n", csvfname);
	}
	fprintf(csvf, "Benchmark;Procs;Shared;Sim_time;L1 miss%%;L2 miss%%;L3 miss%%;");
	fprintf(csvf, "L3 fetch;L3 idle;L3 wait;Mem fetch;Mem idle;Mem wait;");
	fprintf(csvf, "L3 busy%%;L3 wait%%;Mem busy%%;Mem wait%%;Instructions;Cycles;Cyc/Instr\n");
	
	switch (select) {
		case 1:
			for (bench = 4; bench < 12; bench++) {
				for (pr = 1; pr <= 8; pr++) {
					stat = dobench(pr, benches1[bench], 0);
					if (stat) {
						printf("Non-zero status, %d, bench %s, pr %d, non-share\n", stat, benches1[bench], pr);
						return stat;
					}
				}
				for (pr = 2; pr <= 8; pr++) {
					stat = dobench(pr, benches1[bench], 1);
					if (stat) {
						printf("Non-zero status, %d, bench %s, pr %d, shared\n", stat, benches1[bench], pr);
						return stat;
					}
				}
			}
			break;
			
		case 2:
			for (bench = 2; bench < 13; bench++) {
				for (pr = 1; pr <= 8; pr++) {
					stat = dobench(pr, benches2[bench], 0);
					if (stat) {
						printf("Non-zero status, %d, bench %s, pr %d, non-share\n", stat, benches2[bench], pr);
						return stat;
					}
				}
				for (pr = 2; pr <= 8; pr++) {
					stat = dobench(pr, benches2[bench], 1);
					if (stat) {
						printf("Non-zero status, %d, bench %s, pr %d, shared\n", stat, benches2[bench], pr);
						return stat;
					}
				}
			}
			break;
			
		case 3:
			for (pr = 1; pr <= 8; pr++) {
				stat = dobench(pr, benches3[0], 0);
				if (stat) {
					printf("Non-zero status, %d, bench %s, pr %d, non-share\n", stat, benches3[0], pr);
					return stat;
				}
			}
			for (pr = 2; pr <= 8; pr++) {
				stat = dobench(pr, benches3[0], 1);
				if (stat) {
					printf("Non-zero status, %d, bench %s, pr %d, shared\n", stat, benches3[0], pr);
					return stat;
				}
			}
			break;
			
		case 4:
			for (pr = 1; pr <= 8; pr++) {
				stat = dobench(pr, benches4[0], 0);
				if (stat) {
					printf("Non-zero status, %d, bench %s, pr %d, non-share\n", stat, benches4[0], pr);
					return stat;
				}
			}
			for (pr = 2; pr <= 8; pr++) {
				stat = dobench(pr, benches4[0], 1);
				if (stat) {
					printf("Non-zero status, %d, bench %s, pr %d, shared\n", stat, benches4[0], pr);
					return stat;
				}
			}
			break;
			
		case 5:
			for (pr = 1; pr <= 8; pr++) {
				stat = dobench(pr, benches5[0], 0);
				if (stat) {
					printf("Non-zero status, %d, bench %s, pr %d, non-share\n", stat, benches5[0], pr);
					return stat;
				}
			}
			for (pr = 2; pr <= 8; pr++) {
				stat = dobench(pr, benches5[0], 1);
				if (stat) {
					printf("Non-zero status, %d, bench %s, pr %d, shared\n", stat, benches5[0], pr);
					return stat;
				}
			}
			break;
			
		case 6:
			for (pr = 1; pr <= 8; pr++) {
				stat = dobench(pr, benches6[0], 0);
				if (stat) {
					printf("Non-zero status, %d, from 'dobench' bench %s, pr %d\n", stat, benches6[0], pr);
					return stat;
				}
			}
			for (pr = 2; pr <= 8; pr++) {
				stat = dobench(pr, benches6[0], 1);
				if (stat) {
					printf("Non-zero status, %d, from 'dobench' bench %s, pr %d\n", stat, benches6[0], pr);
					return stat;
				}
			}
			break;
			
		default:
			printf("ERROR: %d is not a valid benchmark group; select 1-6\n", select);
			break;
	}
	//	fclose(csvf);
	return 0;
}
*/
