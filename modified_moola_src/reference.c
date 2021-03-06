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
#include <limits.h>
#include <math.h>
#include "moola.h"
#include "des.h"
#include <inttypes.h>
//#include <openssl/sha.h>
//#define SIZE  10000000
//uint64_t v[SIZE];
//uint64_t vector_index=0;

//counter =0;


#define SIZE 1000000000
//#define SET_BITS 6   // set_lines = 2 Pow 6 
#define SCHEMA_NUMBER 6
//#define VECTOR_SIZE 9000000000
uint64_t a;
int s;
//int scheme=0;


static uint32_t rotr(uint32_t n, uint32_t num_bits, unsigned int c) {
        if (!c) {
                return n;
        }
        uint32_t mask = (1 << c) - 1;
        uint32_t lower = n & mask;
        n >>= c;
        lower <<= (num_bits - c);
        n |= lower;
        return n;
}

static uint32_t rotl(uint64_t n, uint32_t num_bits, unsigned int c) {
        if (!c) {
                return n;
        }
        uint32_t mask = ((1 << c) - 1) << (num_bits - c);
        uint32_t upper = n & mask;
        n = (n &= ~upper) << c;
        upper >>= (num_bits - c);
        n |= upper;
        return n;
}
/*
uint64_t special_rot(uint64_t a, uint32_t num_bits, unsigned int c) {
        int set_lines = pow(2.0,num_bits);
        int s = a & (set_lines-1);
	if (!c) {
                return s;
        }
	uint32_t new_set = (a>c) & (set_lines-1);
	uint32_t low_mask = (1 << c) - 1;
	uint32_t lower_s = s & low_mask;
	uint64_t tag = a >> num_bits ;
//      uint32_t lower_tag =  tag & low_mask;  
	uint64_t new_tag = tag >> c;
	new_tag = new_tag  & (set_lines-1);       	
	uint64_t  final_tag =   (new_tag << c) | lower_s;
        return ( new_set ^ final_tag); 	
}
*/

int  intel_slide_case(uint64_t a){
        unsigned int i;
        unsigned int A[35];
	// asign the A[] values to the position
	for (i=6;i<35;i++){
		A[i] = a>>i & 1;
	}

	// First step
	unsigned int F[7];
	F[0] = A[6]^A[12]^A[17]^A[18]^A[22]^A[24]^A[27]^A[29]^A[30]^A[32];
	F[1] = A[7]^A[12]^A[13]^A[17]^A[19]^A[22]^A[23]^A[24]^A[25]^A[27]^A[28]^A[31]^A[32]^A[33];	
	F[2] = A[8]^A[13]^A[14]^A[18]^A[20]^A[23]^A[24]^A[25]^A[26]^A[28]^A[32]^A[33]^A[34];
	F[3] = A[9]^A[14]^A[15]^A[19]^A[21]^A[24]^A[25]^A[26]^A[27]^A[29]^A[33]^A[34];
	F[4] = A[10]^A[15]^A[16]^A[20]^A[22]^A[25]^A[26]^A[27]^A[28]^A[30]^A[34];
	F[5] = A[11]^A[16]^A[17]^A[21]^A[23]^A[26]^A[27]^A[28]^A[29]^A[31];
	F[6] = A[12]^A[13]^A[14]^A[15]^A[16]^A[18]^A[19]^A[20]^A[21]^A[24]^A[25]^A[26]^A[28]^A[30]^A[31];

	// Second step
	unsigned int S[3];
	S[2] =  (F[0]^F[5])&( (F[2]|F[3]) & ((F[4]|F[5]))) ;
	S[1] =  F[1]&(~S[2]);
	S[0] =  F[0]^F[1]^F[2]^F[3]^F[4]^F[5]^F[6];

	unsigned int  s=0;
	s = (S[2]<<2) | (S[1]<<1) |  S[0];	
//	printf("%d%d%d  %d \n", S[2],S[1], S[0], s);
	return s;	
}

void  generate_rand(uint64_t *des_key) {
    int i;

    uint64_t r = 0;
    // Generate RND 48 bits number
    for (i = 0; i < 8; ++i) {
        r = (r << 12) | (rand()& 0xFF);
    }
    // Apply mask to be sure that is 48 bits
    *des_key = r & 0xFFFFFFFFFFFFFFFF; 
    return ;
}


uint64_t ceaser_address(uint64_t a, uint32_t key1, uint32_t key2, uint32_t key3, uint32_t key4 ){
        // a has 20 bits. Divive by L and R , 20 bits each
        // Key has 80 bit =  k1,k2.k3,k4
        uint32_t L=0, final_L=0;
        uint32_t R=0, final_R=0; 
	uint32_t R1=0,R2=0,R3=0;
	uint32_t L1=0,L2=0,L3=0;
	unsigned int r1[20], r2[20], r3[20];
	unsigned int l1[20], l2[20], l3[20];
	unsigned int s1[20], s2[20], s3[20], s4[20];
	unsigned int y1[20], y2[20], y3[20], y4[20];
	unsigned int k1[20], k2[20], k3[20], k4[20];
	uint64_t final_a=0;	

	R = a & 0xFFFFF;
	L = (a >> 20);

	// printf("address = 0x%lx  L = 0x%lx  R = 0x%lx  \n", a,L,R);

	// Precalc of bits 
	unsigned int i;
        // asign the k[] values to the bit position. Initialization 
        for (i=0;i<20;i++){
                k1[i] = key1>>i & 0x1;
		k2[i] = key2>>i & 0x1;
		k3[i] = key3>>i & 0x1;
		k4[i] = key4>>i & 0x1;
	        r1[i] = R>>i & 0x1;
		l1[i] = L>>i & 0x1;
		s1[i] = 0;
		s2[i] = 0;
		s3[i] = 0;
		s4[i] = 0;
		y1[i] = 0;
                y2[i] = 0;
                y3[i] = 0;
                y4[i] = 0;

        }

	////// ------- STAGE 1 ----------------------------------------------- ///
	// first S-Box
	s1[0]= l1[0]^l1[1]^l1[2]^l1[3]^l1[4]^l1[5]^l1[6]^l1[7]^l1[8]^l1[9] ^ k1[0]^k1[1]^k1[2]^k1[3]^k1[4]^k1[5]^k1[6]^k1[7]^k1[8]^k1[9];
	s1[1]= l1[0]^l1[1]^l1[2]^l1[4]^l1[5]^l1[7]^l1[9]^l1[11]^l1[18]^l1[19] ^ k1[1]^k1[3]^k1[5]^k1[7]^k1[9]^k1[11]^k1[13]^k1[15]^k1[17]^k1[19];
	s1[2]= l1[1]^l1[3]^l1[5]^l1[7]^l1[9]^l1[11]^l1[13]^l1[15]^l1[17]^l1[19] ^ k1[0]^k1[1]^k1[2]^k1[3]^k1[4]^k1[5]^k1[6]^k1[7]^k1[8]^k1[9];
	s1[3]= l1[0]^l1[2]^l1[4]^l1[6]^l1[8]^l1[10]^l1[12]^l1[14]^l1[16]^l1[18] ^ k1[0]^k1[1]^k1[2]^k1[3]^k1[7]^k1[9]^k1[11]^k1[12]^k1[14]^k1[17];
	s1[4]= l1[1]^l1[3]^l1[8]^l1[9]^l1[11]^l1[12]^l1[14]^l1[17]^l1[18]^l1[19] ^ k1[2]^k1[4]^k1[6]^k1[7]^k1[9]^k1[11]^k1[12]^k1[15]^k1[17]^k1[18];
	s1[5]= l1[8]^l1[9]^l1[10]^l1[11]^l1[14]^l1[15]^l1[16]^l1[17]^l1[18]^l1[19] ^ k1[3]^k1[4]^k1[5]^k1[7]^k1[9]^k1[12]^k1[13]^k1[14]^k1[16]^k1[17];
	s1[6]= l1[1]^l1[4]^l1[6]^l1[7]^l1[8]^l1[10]^l1[11]^l1[14]^l1[15]^l1[19] ^ k1[2]^k1[5]^k1[7]^k1[8]^k1[12]^k1[13]^k1[16]^k1[17]^k1[18]^k1[19];
	s1[7]= l1[4]^l1[5]^l1[6]^l1[7]^l1[8]^l1[9]^l1[11]^l1[13]^l1[16]^l1[17] ^ k1[3]^k1[5]^k1[6]^k1[8]^k1[9]^k1[11]^k1[12]^k1[15]^k1[16]^k1[17];
	s1[8]= l1[5]^l1[6]^l1[7]^l1[9]^l1[12]^l1[14]^l1[15]^l1[16]^l1[18]^l1[19] ^ k1[6]^k1[7]^k1[8]^k1[9]^k1[11]^k1[12]^k1[13]^k1[14]^k1[16]^k1[17];
	s1[9]= l1[6]^l1[7]^l1[8]^l1[9]^l1[12]^l1[13]^l1[14]^l1[15]^l1[17]^l1[19] ^ k1[4]^k1[5]^k1[6]^k1[7]^k1[9]^k1[11]^k1[12]^k1[13]^k1[14]^k1[15];	
	s1[10]= l1[7]^l1[8]^l1[9]^l1[10]^l1[11]^l1[13]^l1[14]^l1[15]^l1[16]^l1[17] ^ k1[10]^k1[11]^k1[12]^k1[13]^k1[14]^k1[15]^k1[16]^k1[17]^k1[18]^k1[19];
        s1[11]= l1[8]^l1[9]^l1[10]^l1[11]^l1[12]^l1[13]^l1[14]^l1[15]^l1[16]^l1[17] ^ k1[0]^k1[3]^k1[5]^k1[7]^k1[9]^k1[12]^k1[13]^k1[16]^k1[17]^k1[19];
        s1[12]= l1[9]^l1[10]^l1[11]^l1[12]^l1[13]^l1[14]^l1[15]^l1[16]^l1[17]^l1[18] ^ k1[0]^k1[1]^k1[2]^k1[3]^k1[4]^k1[5]^k1[8]^k1[9]^k1[11]^k1[13];
        s1[13]= l1[3]^l1[5]^l1[7]^l1[8]^l1[9]^l1[10]^l1[13]^l1[14]^l1[16]^l1[18] ^ k1[0]^k1[1]^k1[2]^k1[4]^k1[7]^k1[9]^k1[11]^k1[12]^k1[13]^k1[17];
        s1[14]= l1[1]^l1[3]^l1[7]^l1[9]^l1[11]^l1[13]^l1[14]^l1[15]^l1[18]^l1[19] ^ k1[2]^k1[4]^k1[6]^k1[7]^k1[8]^k1[11]^k1[13]^k1[15]^k1[17]^k1[18];
        s1[15]= l1[8]^l1[9]^l1[10]^l1[11]^l1[13]^l1[14]^l1[16]^l1[17]^l1[18]^l1[19] ^ k1[3]^k1[4]^k1[5]^k1[7]^k1[9]^k1[12]^k1[13]^k1[14]^k1[16]^k1[17];
        s1[16]= l1[1]^l1[3]^l1[6]^l1[7]^l1[8]^l1[9]^l1[11]^l1[14]^l1[15]^l1[16] ^ k1[2]^k1[3]^k1[7]^k1[8]^k1[12]^k1[14]^k1[16]^k1[17]^k1[18]^k1[19];
        s1[17]= l1[4]^l1[5]^l1[6]^l1[7]^l1[8]^l1[9]^l1[14]^l1[15]^l1[16]^l1[19] ^ k1[3]^k1[4]^k1[6]^k1[8]^k1[9]^k1[10]^k1[12]^k1[13]^k1[16]^k1[17];
        s1[18]= l1[5]^l1[6]^l1[7]^l1[8]^l1[12]^l1[14]^l1[15]^l1[16]^l1[18]^l1[19] ^ k1[6]^k1[7]^k1[8]^k1[9]^k1[10]^k1[11]^k1[13]^k1[14]^k1[16]^k1[17];
        s1[19]= l1[6]^l1[7]^l1[8]^l1[9]^l1[11]^l1[12]^l1[14]^l1[15]^l1[17]^l1[18] ^ k1[4]^k1[5]^k1[6]^k1[8]^k1[9]^k1[11]^k1[12]^k1[14]^k1[15]^k1[18];


	// first P-BOX		

	y1[0] = s1[0];
	y1[1] = s1[2];
	y1[2] = s1[3];
	y1[3] = s1[4];
	y1[4] = s1[5];
	y1[5] = s1[6];
	y1[6] = s1[7];
	y1[7] = s1[8];
	y1[8] = s1[9];
	y1[9] = s1[10];
	y1[10] = s1[11];
	y1[11] = s1[12];
	y1[12] = s1[13];
	y1[13] = s1[14];
	y1[14] = s1[15];
	y1[15] = s1[16];
	y1[16] = s1[17];
	y1[17] = s1[18];
	y1[18] = s1[19];
	y1[19] = s1[1];

	// XOR Operation
	 for (i=0;i<20;i++){
		r2[i] = r1[i]^y1[i];   
	}


	////// ------- STAGE 2 ----------------------------------------------- ///

	// Second  S-Box
	s2[0]= r2[5]^r2[6]^r2[7]^r2[8]^r2[12]^r2[14]^r2[15]^r2[16]^r2[18]^r2[19] ^ k2[6]^k2[7]^k2[8]^k2[9]^k2[10]^k2[11]^k2[13]^k2[14]^k2[16]^k2[17];
        s2[1]= r2[6]^r2[7]^r2[8]^r2[9]^r2[11]^r2[12]^r2[14]^r2[15]^r2[17]^r2[18] ^ k2[4]^k2[5]^k2[6]^k2[8]^k2[9]^k2[11]^k2[12]^k2[14]^k2[15]^k2[18];
	s2[2]= r2[0]^r2[1]^r2[2]^r2[3]^r2[4]^r2[5]^r2[6]^r2[7]^r2[8]^r2[9] ^ k2[0]^k2[1]^k2[2]^k2[3]^k2[4]^k2[5]^k2[6]^k2[7]^k2[8]^k2[9];
	s2[3]= r2[0]^r2[1]^r2[2]^r2[4]^r2[5]^r2[7]^r2[9]^r2[11]^r2[18]^r2[19] ^ k2[1]^k2[3]^k2[5]^k2[7]^k2[9]^k2[11]^k2[13]^k2[15]^k2[17]^k2[19];
	s2[4]= r2[1]^r2[3]^r2[5]^r2[7]^r2[9]^r2[11]^r2[13]^r2[15]^r2[17]^r2[19] ^ k2[0]^k2[1]^k2[2]^k2[3]^k2[4]^k2[5]^k2[6]^k2[7]^k2[8]^k2[9];
	s2[5]= r2[0]^r2[2]^r2[4]^r2[6]^r2[8]^r2[10]^r2[12]^r2[14]^r2[16]^r2[18] ^ k2[0]^k2[1]^k2[2]^k2[3]^k2[7]^k2[9]^k2[11]^k2[12]^k2[14]^k2[17];
	s2[6]= r2[1]^r2[3]^r2[8]^r2[9]^r2[11]^r2[12]^r2[14]^r2[17]^r2[18]^r2[19] ^ k2[2]^k2[4]^k2[6]^k2[7]^k2[9]^k2[11]^k2[12]^k2[15]^k2[17]^k2[18];
	s2[7]= r2[8]^r2[9]^r2[10]^r2[11]^r2[14]^r2[15]^r2[16]^r2[17]^r2[18]^r2[19] ^ k2[3]^k2[4]^k2[5]^k2[7]^k2[9]^k2[12]^k2[13]^k2[14]^k2[16]^k2[17];
	s2[8]= r2[1]^r2[4]^r2[6]^r2[7]^r2[8]^r2[10]^r2[11]^r2[14]^r2[15]^r2[19] ^ k2[2]^k2[5]^k2[7]^k2[8]^k2[12]^k2[13]^k2[16]^k2[17]^k2[18]^k2[19];
	s2[9]= r2[4]^r2[5]^r2[6]^r2[7]^r2[8]^r2[9]^r2[11]^r2[13]^r2[16]^r2[17] ^ k2[3]^k2[5]^k2[6]^k2[8]^k2[9]^k2[11]^k2[12]^k2[15]^k2[16]^k2[17];
	s2[10]= r2[5]^r2[6]^r2[7]^r2[9]^r2[12]^r2[14]^r2[15]^r2[16]^r2[18]^r2[19] ^ k2[6]^k2[7]^k2[8]^k2[9]^k2[11]^k2[12]^k2[13]^k2[14]^k2[16]^k2[17];
	s2[11]= r2[6]^r2[7]^r2[8]^r2[9]^r2[12]^r2[13]^r2[14]^r2[15]^r2[17]^r2[19] ^ k2[4]^k2[5]^k2[6]^k2[7]^k2[9]^k2[11]^k2[12]^k2[13]^k2[14]^k2[15];	
	s2[12]= r2[7]^r2[8]^r2[9]^r2[10]^r2[11]^r2[13]^r2[14]^r2[15]^r2[16]^r2[17] ^ k2[10]^k2[11]^k2[12]^k2[13]^k2[14]^k2[15]^k2[16]^k2[17]^k2[18]^k2[19];
        s2[13]= r2[8]^r2[9]^r2[10]^r2[11]^r2[12]^r2[13]^r2[14]^r2[15]^r2[16]^r2[17] ^ k2[0]^k2[3]^k2[5]^k2[7]^k2[9]^k2[12]^k2[13]^k2[16]^k2[17]^k2[19];
        s2[14]= r2[9]^r2[10]^r2[11]^r2[12]^r2[13]^r2[14]^r2[15]^r2[16]^r2[17]^r2[18] ^ k2[0]^k2[1]^k2[2]^k2[3]^k2[4]^k2[5]^k2[8]^k2[9]^k2[11]^k2[13];
        s2[15]= r2[3]^r2[5]^r2[7]^r2[8]^r2[9]^r2[10]^r2[13]^r2[14]^r2[16]^r2[18] ^ k2[0]^k2[1]^k2[2]^k2[4]^k2[7]^k2[9]^k2[11]^k2[12]^k2[13]^k2[17];
        s2[16]= r2[1]^r2[3]^r2[7]^r2[9]^r2[11]^r2[13]^r2[14]^r2[15]^r2[18]^r2[19] ^ k2[2]^k2[4]^k2[6]^k2[7]^k2[8]^k2[11]^k2[13]^k2[15]^k2[17]^k2[18];
        s2[17]= r2[8]^r2[9]^r2[10]^r2[11]^r2[13]^r2[14]^r2[16]^r2[17]^r2[18]^r2[19] ^ k2[3]^k2[4]^k2[5]^k2[7]^k2[9]^k2[12]^k2[13]^k2[14]^k2[16]^k2[17];
        s2[18]= r2[1]^r2[3]^r2[6]^r2[7]^r2[8]^r2[9]^r2[11]^r2[14]^r2[15]^r2[16] ^ k2[2]^k2[3]^k2[7]^k2[8]^k2[12]^k2[14]^k2[16]^k2[17]^k2[18]^k2[19];
        s2[19]= r2[4]^r2[5]^r2[6]^r2[7]^r2[8]^r2[9]^r2[14]^r2[15]^r2[16]^r2[19] ^ k2[3]^k2[4]^k2[6]^k2[8]^k2[9]^k2[10]^k2[12]^k2[13]^k2[16]^k2[17];
	
        // second  P-BOX

        y2[0] = s2[2];
        y2[1] = s2[3];
        y2[2] = s2[4];
        y2[3] = s2[5];
        y2[4] = s2[6];
        y2[5] = s2[7];
        y2[6] = s2[8];
        y2[7] = s2[9];
        y2[8] = s2[10];
        y2[9] = s2[11];
        y2[10] = s2[12];
        y2[11] = s2[13];
        y2[12] = s2[14];
        y2[13] = s2[15];
        y2[14] = s2[16];
        y2[15] = s2[17];
        y2[16] = s2[18];
        y2[17] = s2[19];
        y2[18] = s2[1];
        y2[19] = s2[2];
        
	 // XOR Operation
         for (i=0;i<20;i++){
                l2[i] = l1[i]^y2[i];
        }

	////// ------- STAGE 3 ----------------------------------------------- ///

	// third S-Box
	s3[0]= l2[8]^l2[9]^l2[10]^l2[11]^l2[13]^l2[14]^l2[16]^l2[17]^l2[18]^l2[19] ^ k3[3]^k3[4]^k3[5]^k3[7]^k3[9]^k3[12]^k3[13]^k3[14]^k3[16]^k3[17];
    	s3[1]= l2[1]^l2[3]^l2[6]^l2[7]^l2[8]^l2[9]^l2[11]^l2[14]^l2[15]^l2[16] ^ k3[2]^k3[3]^k3[7]^k3[8]^k3[12]^k3[14]^k3[16]^k3[17]^k3[18]^k3[19];
	s3[2]= l2[4]^l2[5]^l2[6]^l2[7]^l2[8]^l2[9]^l2[14]^l2[15]^l2[16]^l2[19] ^ k3[3]^k3[4]^k3[6]^k3[8]^k3[9]^k3[10]^k3[12]^k3[13]^k3[16]^k3[17];
	s3[3]= l2[5]^l2[6]^l2[7]^l2[8]^l2[12]^l2[14]^l2[15]^l2[16]^l2[18]^l2[19] ^ k3[6]^k3[7]^k3[8]^k3[9]^k3[10]^k3[11]^k3[13]^k3[14]^k3[16]^k3[17];
        s3[4]= l2[6]^l2[7]^l2[8]^l2[9]^l2[11]^l2[12]^l2[14]^l2[15]^l2[17]^l2[18] ^ k3[4]^k3[5]^k3[6]^k3[8]^k3[9]^k3[11]^k3[12]^k3[14]^k3[15]^k3[18];
	s3[5]= l2[0]^l2[1]^l2[2]^l2[3]^l2[4]^l2[5]^l2[6]^l2[7]^l2[8]^l2[9] ^ k3[0]^k3[1]^k3[2]^k3[3]^k3[4]^k3[5]^k3[6]^k3[7]^k3[8]^k3[9];
	s3[6]= l2[0]^l2[1]^l2[2]^l2[4]^l2[5]^l2[7]^l2[9]^l2[11]^l2[18]^l2[19] ^ k3[1]^k3[3]^k3[5]^k3[7]^k3[9]^k3[11]^k3[13]^k3[15]^k3[17]^k3[19];
	s3[7]= l2[1]^l2[3]^l2[5]^l2[7]^l2[9]^l2[11]^l2[13]^l2[15]^l2[17]^l2[19] ^ k3[0]^k3[1]^k3[2]^k3[3]^k3[4]^k3[5]^k3[6]^k3[7]^k3[8]^k3[9];
	s3[8]= l2[0]^l2[2]^l2[4]^l2[6]^l2[8]^l2[10]^l2[12]^l2[14]^l2[16]^l2[18] ^ k3[0]^k3[1]^k3[2]^k3[3]^k3[7]^k3[9]^k3[11]^k3[12]^k3[14]^k3[17];
	s3[9]= l2[1]^l2[3]^l2[8]^l2[9]^l2[11]^l2[12]^l2[14]^l2[17]^l2[18]^l2[19] ^ k3[2]^k3[4]^k3[6]^k3[7]^k3[9]^k3[11]^k3[12]^k3[15]^k3[17]^k3[18];
	s3[10]= l2[8]^l2[9]^l2[10]^l2[11]^l2[14]^l2[15]^l2[16]^l2[17]^l2[18]^l2[19] ^ k3[3]^k3[4]^k3[5]^k3[7]^k3[9]^k3[12]^k3[13]^k3[14]^k3[16]^k3[17];
	s3[11]= l2[1]^l2[4]^l2[6]^l2[7]^l2[8]^l2[10]^l2[11]^l2[14]^l2[15]^l2[19] ^ k3[2]^k3[5]^k3[7]^k3[8]^k3[12]^k3[13]^k3[16]^k3[17]^k3[18]^k3[19];
	s3[12]= l2[4]^l2[5]^l2[6]^l2[7]^l2[8]^l2[9]^l2[11]^l2[13]^l2[16]^l2[17] ^ k3[3]^k3[5]^k3[6]^k3[8]^k3[9]^k3[11]^k3[12]^k3[15]^k3[16]^k3[17];
	s3[13]= l2[5]^l2[6]^l2[7]^l2[9]^l2[12]^l2[14]^l2[15]^l2[16]^l2[18]^l2[19] ^ k3[6]^k3[7]^k3[8]^k3[9]^k3[11]^k3[12]^k3[13]^k3[14]^k3[16]^k3[17];
	s3[14]= l2[6]^l2[7]^l2[8]^l2[9]^l2[12]^l2[13]^l2[14]^l2[15]^l2[17]^l2[19] ^ k3[4]^k3[5]^k3[6]^k3[7]^k3[9]^k3[11]^k3[12]^k3[13]^k3[14]^k3[15];	
	s3[15]= l2[7]^l2[8]^l2[9]^l2[10]^l2[11]^l2[13]^l2[14]^l2[15]^l2[16]^l2[17] ^ k3[10]^k3[11]^k3[12]^k3[13]^k3[14]^k3[15]^k3[16]^k3[17]^k3[18]^k3[19];
        s3[16]= l2[8]^l2[9]^l2[10]^l2[11]^l2[12]^l2[13]^l2[14]^l2[15]^l2[16]^l2[17] ^ k3[0]^k3[3]^k3[5]^k3[7]^k3[9]^k3[12]^k3[13]^k3[16]^k3[17]^k3[19];
        s3[17]= l2[9]^l2[10]^l2[11]^l2[12]^l2[13]^l2[14]^l2[15]^l2[16]^l2[17]^l2[18] ^ k3[0]^k3[1]^k3[2]^k3[3]^k3[4]^k3[5]^k3[8]^k3[9]^k3[11]^k3[13];
        s3[18]= l2[3]^l2[5]^l2[7]^l2[8]^l2[9]^l2[10]^l2[13]^l2[14]^l2[16]^l2[18] ^ k3[0]^k3[1]^k3[2]^k3[4]^k3[7]^k3[9]^k3[11]^k3[12]^k3[13]^k3[17];
        s3[19]= l2[1]^l2[3]^l2[7]^l2[9]^l2[11]^l2[13]^l2[14]^l2[15]^l2[18]^l2[19] ^ k3[2]^k3[4]^k3[6]^k3[7]^k3[8]^k3[11]^k3[13]^k3[15]^k3[17]^k3[18];
          
	// third  P-BOX
        y3[0] = s3[10];
        y3[1] = s3[11];
        y3[2] = s3[12];
        y3[3] = s3[13];
        y3[4] = s3[14];
        y3[5] = s3[15];
        y3[6] = s3[16];
        y3[7] = s3[17];
        y3[8] = s3[18];
        y3[9] = s3[0];
        y3[10] = s3[1];
        y3[11] = s3[2];
        y3[12] = s3[3];
        y3[13] = s3[4];
        y3[14] = s3[5];
        y3[15] = s3[6];
        y3[16] = s3[7];
        y3[17] = s3[8];
        y3[18] = s3[9];
        y3[19] = s3[19];

	 // XOR Operation
         for (i=0;i<20;i++){
                l3[i] = r2[i]^y3[i];
        }


	////// ------- STAGE 4 ----------------------------------------------- ///

	// fourth S-Box
	s4[0]= l3[1]^l3[3]^l3[7]^l3[9]^l3[11]^l3[13]^l3[14]^l3[15]^l3[18]^l3[19] ^ k4[2]^k4[4]^k4[6]^k4[7]^k4[8]^k4[11]^k4[13]^k4[15]^k4[17]^k4[18];
  	s4[1]= l3[8]^l3[9]^l3[10]^l3[11]^l3[13]^l3[14]^l3[16]^l3[17]^l3[18]^l3[19] ^ k4[3]^k4[4]^k4[5]^k4[7]^k4[9]^k4[12]^k4[13]^k4[14]^k4[16]^k4[17];
        s4[3]= l3[1]^l3[3]^l3[6]^l3[7]^l3[8]^l3[9]^l3[11]^l3[14]^l3[15]^l3[16] ^ k4[2]^k4[3]^k4[7]^k4[8]^k4[12]^k4[14]^k4[16]^k4[17]^k4[18]^k4[19];
        s4[3]= l3[4]^l3[5]^l3[6]^l3[7]^l3[8]^l3[9]^l3[14]^l3[15]^l3[16]^l3[19] ^ k4[3]^k4[4]^k4[6]^k4[8]^k4[9]^k4[10]^k4[12]^k4[13]^k4[16]^k4[17];
        s4[4]= l3[5]^l3[6]^l3[7]^l3[8]^l3[12]^l3[14]^l3[15]^l3[16]^l3[18]^l3[19] ^ k4[6]^k4[7]^k4[8]^k4[9]^k4[10]^k4[11]^k4[13]^k4[14]^k4[16]^k4[17];
        s4[5]= l3[6]^l3[7]^l3[8]^l3[9]^l3[11]^l3[12]^l3[14]^l3[15]^l3[17]^l3[18] ^ k4[4]^k4[5]^k4[6]^k4[8]^k4[9]^k4[11]^k4[12]^k4[14]^k4[15]^k4[18];
	s4[6]= l3[0]^l3[1]^l3[2]^l3[3]^l3[4]^l3[5]^l3[6]^l3[7]^l3[8]^l3[9] ^ k4[0]^k4[1]^k4[2]^k4[3]^k4[4]^k4[5]^k4[6]^k4[7]^k4[8]^k4[9];
	s4[7]= l3[0]^l3[1]^l3[2]^l3[4]^l3[5]^l3[7]^l3[9]^l3[11]^l3[18]^l3[19] ^ k4[1]^k4[3]^k4[5]^k4[7]^k4[9]^k4[11]^k4[13]^k4[15]^k4[17]^k4[19];
	s4[8]= l3[1]^l3[3]^l3[5]^l3[7]^l3[9]^l3[11]^l3[13]^l3[15]^l3[17]^l3[19] ^ k4[0]^k4[1]^k4[2]^k4[3]^k4[4]^k4[5]^k4[6]^k4[7]^k4[8]^k4[9];
	s4[9]= l3[0]^l3[2]^l3[4]^l3[6]^l3[8]^l3[10]^l3[12]^l3[14]^l3[16]^l3[18] ^ k4[0]^k4[1]^k4[2]^k4[3]^k4[7]^k4[9]^k4[11]^k4[12]^k4[14]^k4[17];
	s4[10]= l3[1]^l3[3]^l3[8]^l3[9]^l3[11]^l3[12]^l3[14]^l3[17]^l3[18]^l3[19] ^ k4[2]^k4[4]^k4[6]^k4[7]^k4[9]^k4[11]^k4[12]^k4[15]^k4[17]^k4[18];
	s4[11]= l3[8]^l3[9]^l3[10]^l3[11]^l3[14]^l3[15]^l3[16]^l3[17]^l3[18]^l3[19] ^ k4[3]^k4[4]^k4[5]^k4[7]^k4[9]^k4[12]^k4[13]^k4[14]^k4[16]^k4[17];
	s4[12]= l3[1]^l3[4]^l3[6]^l3[7]^l3[8]^l3[10]^l3[11]^l3[14]^l3[15]^l3[19] ^ k4[2]^k4[5]^k4[7]^k4[8]^k4[12]^k4[13]^k4[16]^k4[17]^k4[18]^k4[19];
	s4[13]= l3[4]^l3[5]^l3[6]^l3[7]^l3[8]^l3[9]^l3[11]^l3[13]^l3[16]^l3[17] ^ k4[3]^k4[5]^k4[6]^k4[8]^k4[9]^k4[11]^k4[12]^k4[15]^k4[16]^k4[17];
	s4[14]= l3[5]^l3[6]^l3[7]^l3[9]^l3[12]^l3[14]^l3[15]^l3[16]^l3[18]^l3[19] ^ k4[6]^k4[7]^k4[8]^k4[9]^k4[11]^k4[12]^k4[13]^k4[14]^k4[16]^k4[17];
	s4[15]= l3[6]^l3[7]^l3[8]^l3[9]^l3[12]^l3[13]^l3[14]^l3[15]^l3[17]^l3[19] ^ k4[4]^k4[5]^k4[6]^k4[7]^k4[9]^k4[11]^k4[12]^k4[13]^k4[14]^k4[15];	
	s4[16]= l3[7]^l3[8]^l3[9]^l3[10]^l3[11]^l3[13]^l3[14]^l3[15]^l3[16]^l3[17] ^ k4[10]^k4[11]^k4[12]^k4[13]^k4[14]^k4[15]^k4[16]^k4[17]^k4[18]^k4[19];
        s4[17]= l3[8]^l3[9]^l3[10]^l3[11]^l3[12]^l3[13]^l3[14]^l3[15]^l3[16]^l3[17] ^ k4[0]^k4[3]^k4[5]^k4[7]^k4[9]^k4[12]^k4[13]^k4[16]^k4[17]^k4[19];
        s4[18]= l3[9]^l3[10]^l3[11]^l3[12]^l3[13]^l3[14]^l3[15]^l3[16]^l3[17]^l3[18] ^ k4[0]^k4[1]^k4[2]^k4[3]^k4[4]^k4[5]^k4[8]^k4[9]^k4[11]^k4[13];
        s4[19]= l3[3]^l3[5]^l3[7]^l3[8]^l3[9]^l3[10]^l3[13]^l3[14]^l3[16]^l3[18] ^ k4[0]^k4[1]^k4[2]^k4[4]^k4[7]^k4[9]^k4[11]^k4[12]^k4[13]^k4[17];
   
	// fourth  P-BOX

        y4[0] = s4[4];
        y4[1] = s4[5];
        y4[2] = s4[6];
        y4[3] = s4[7];
        y4[4] = s4[8];
        y4[5] = s4[9];
        y4[6] = s4[10];
        y4[7] = s4[11];
        y4[8] = s4[12];
        y4[9] = s4[13];
        y4[10] = s4[14];
        y4[11] = s4[15];
        y4[12] = s4[16];
        y4[13] = s4[17];
        y4[14] = s4[18];
        y4[15] = s4[19];
        y4[16] = s4[0];
        y4[17] = s4[1];
        y4[18] = s4[2];
        y4[19] = s4[3];

	 // XOR Operation
         for (i=0;i<20;i++){
                r3[i] = l2[i]^y4[i];
        }

       // printf("%lld ,%lld \n",R,L);
/*
        for (i=0;i<20;i++){
                printf("%d",r3[i]);
        }
        printf("\n");
*/

	// Generate the final address with l3 and r3 as L' and R' respectively
	 for (i=0;i<20;i++){
		final_L += l3[i] << i; 		
		final_R += r3[i] << i;
	}
	final_a = (final_L << 20) |  final_R;
		
//	printf("%lld , %lld \n",a, final_a);

	return final_a;		
}

uint64_t permutation_tag (uint64_t tag, unsigned int set_bits){
	int i, j;
	int tag_part[set_bits];
        int p_tag_part[set_bits];
	uint64_t permuted_tag =0;

	for (i=0;i<set_bits;i++){
               tag_part[i] =tag>>i & 0x1;
	}

	// Permutation of bits

	p_tag_part[0] = tag_part[0];
	for (i=1;i<set_bits-1;i++){
		p_tag_part[i] = tag_part[i+1];
	}
	p_tag_part[set_bits-1] = tag_part[1];


	for (i=0;i<set_bits;i++){
		permuted_tag += p_tag_part[i] << i; 		
	}
	return permuted_tag;
}


uint64_t permutation_set (uint32_t set, unsigned int set_bits){
	int i, j;
	int set_part[set_bits];
        int p_set_part[set_bits];
	uint32_t permuted_set =0;

	for (i=0;i<set_bits;i++){
               set_part[i] = set>>i & 0x1;
	}

	// Permutation of bits

	p_set_part[0] = set_part[0];
	for (i=1;i<set_bits-1;i++){
		p_set_part[i] = set_part[i+1];
	}
	p_set_part[set_bits-1] = set_part[1];


	for (i=0;i<set_bits;i++){
		permuted_set += p_set_part[i] << i; 		
	}
	return permuted_set;
}


int compute_set(uint64_t a, int scheme, int set_lines){  
	  int i;
	  int set;
          int new_set;
//        int set_lines = pow(2.0,SET_BITS);
          int set_bits = log2(set_lines); 
//	  printf("Set bits : %lld\n",a );
          uint64_t tag_part;
	  uint64_t permuted_tag;
	  uint64_t tag;
	  uint64_t des_input;
//        uint64_t des_key = 0x0000000000000000;
          uint64_t des_result = 0x0000000000000000;
	  uint64_t square_tag;
          int length;
	  char str[set_bits];
	  set = a & (set_lines-1);
	  uint64_t new_address;
          uint64_t cipher_address;
          uint32_t k1,k2,k3,k4;  // CEASER keys in total 80 bits (20 each)
          k1 = 0xababa;
          k2 = 0xcdcdc;
          k3 = 0xbabab;
          k4 = 0xdcdcd;

//	  printf("set inside: %d\n", set);

	 // unsigned char hash[SHA_DIGEST_LENGTH];
          switch (scheme){
		case 0:  	// Mosule scheme
            		return (int) ( set );
	  	break;
		case 1:       // Rotate Right by 3
                        set = rotr(set,set_bits,3);
                        return ( set);
                break;
               case 2:         //  TAG XOR SET
                        tag_part = (a >> set_bits ) & (set_lines-1);
                        return ( set ^ tag_part);
                break;
		case 3:       // Rotate Right by 1 & XOR
                        set = rotr(set,set_bits,1);
                        tag_part = (a >> set_bits ) & (set_lines-1);
                        return ( set ^ tag_part);
                break;
		case 4:   // Square TAG ( select midle part) XOR with set
			tag_part = (a >> set_bits ) & (set_lines-1);                     
			square_tag = pow(tag_part,2.0);
			tag_part =  square_tag >> ( (int) set_bits/2);
			tag_part = tag_part & (set_lines-1);
			return ( set ^ tag_part);
		break;
		case 5: // Odd multiplier (by 7 )
                        tag_part = (a >> set_bits ) & (set_lines-1);
			set  = (7*tag_part+set)%set_lines; 
			return ( set );
                break;
		case 6: // Case of Intel slide Cache
			new_set = (set & ((1<< set_bits-3)-1)) | ( intel_slide_case(a) <<(set_bits-3) ); 
                        return( new_set);
                break;
		case 7: // DES
                        des_input = a;
			des_result = des(des_input, des_key, 'e');
			new_set = des_result %set_lines;
			return(new_set);
		break;
	
		case 8: // CEASER implementation 
			//uint64_t new_address;
			//uint64_t cipher_address;
			//uint32_t k1,k2,k3,k4;
                        new_address = (a & 0xFFFFFFFFFF);  // select 40 bits address
                        cipher_address= ceaser_address(new_address,k1,k2,k3,k4);
		        //printf("%ld  %d  %d  %d \n", a, set_lines,  set, cipher_address & (set_lines-1) ); 
			return (int) (cipher_address & (set_lines-1) );
                break;

		
		case 9: // TAG PERMUTATION and later XOR SET
			//int p_tag_part[set_bits];
			//uint64_t permuted_tag;
			tag_part = (a >> set_bits ) & (set_lines-1);
			permuted_tag = permutation_tag(tag_part,set_bits);
                        return ( set ^ permuted_tag);
                break;


/*
		case 10: // SHA1
			length = set_bits;
			sprintf(str, "%d", set); // convert integer to String
                       // SHA1(set, length, hash);  // my hash  has 160 bits
		//	new_set =  ( hash[18]<<8 || hash[19] ) & ((1<< set_bits)-1)  ;
		//	return( new_set );

                break;
*/	

	  }
}


double corr(struct sets mysets, int bit1pos, int bit2pos )  {
   double corr;
   uint64_t a,b,c,d;
   // minim numerator
   a = mysets.sum_S[bit1pos][bit2pos];
   b = mysets.sum_D[bit1pos][bit2pos];
   c = (a>b)? b:a; // minimum
   d = (a>b)? a:b; // maximum   
   corr = 1 -(double)((double)c/(double)d);
   return corr;
}


void estimatePs(struct sets *mysets, uint64_t a ){

      int nbit;
      int i, j;
      int temp_nbits[42];
      for(nbit=0;nbit<42;nbit++){
         // calcule if bit nbit is 0 or 1
         if ((a >> nbit)& 0x1){  // if is 1
//	      printf("1");
                mysets->p1[nbit]=1;
                mysets->num_1[nbit] = mysets->num_1[nbit]+ 1;
                temp_nbits[nbit]=1;
                
          } else {   // is 0
//             printf("0");
               mysets->p1[nbit]=0;
	       temp_nbits[nbit]=0;
          }
      }
      mysets->elements = mysets->elements + 1;   // add an element

      /// Calculate the correlation formula
      for (i=0; i<42; i++){
	for (j=0; j<42; j++){
		if (temp_nbits[i]^temp_nbits[j]){  // XOR , if is 10 or 01
			mysets->sum_D[i][j]+=1;
		} else {                          // if is 11 or 00
			mysets->sum_S[i][j]+=1;		
		}
	}
      }


  return;
}



void compute_entropies(struct sets *mysets, int set_lines){
	int s=0, nbit=0;
	int i, j;
        double p1;
	int set_bits = log2(set_lines);
	//calculate the p[i] for each set
	for(s=0; s<set_lines ;s++){
		for (nbit=0;nbit<42; nbit++){				
			if (mysets[s].elements ==0) {
				mysets[s].p1[nbit]= 0;
		        } else {
				mysets[s].p1[nbit]=  (double) mysets[s].num_1[nbit]/(double) mysets[s].elements;
			}
			p1 = mysets[s].p1[nbit];
			// printf("For s = %d  p1[%d] = %f \n", s, nbit, p1);
			if ((p1 != 1.0) && (p1 != 0.0) ){
			     // calculate info leakage by bit in this set
			     mysets[s].info_bit[nbit]= 1 - fabs( p1* log2(p1) + (1.0-p1)* log2(1.0-p1));	

			     // Calculate the total entropy 
		             mysets[s].info += fabs( p1* log2(p1) + (1.0-p1)* log2(1.0-p1) ) ;

			     if (nbit < set_bits ) {
			        mysets[s].set_index_info += fabs( p1* log2(p1) + (1.0-p1)* log2(1.0-p1) ) ;
			     } else {
			        mysets[s].tag_info += fabs( p1* log2(p1) + (1.0-p1)* log2(1.0-p1) ) ;
                             }

			} else {  // if pi == 0 or Pi == 1
			     // mysets[s].info += 0.0;	
			     mysets[s].info_bit[nbit]=1; // because the entropy is 0
			}
		  //printf("%d  %d  %f \n", s, nbit, mysets[s].info_bit[nbit]);
		}
	
	

		// 2nd pass to calculate the correlation bits
		double corr_temp, corr_max;
		for (nbit=0;nbit<42; nbit++){
			corr_max =0;
			for (i=nbit-1;i>=0;i--){
				if (nbit==i){
					corr_temp =0;
				} else {
					corr_temp = corr(mysets[s],nbit,i) * mysets[s].corr_info[i] ; 
				}	
				// take the maximum value
				if (corr_temp > corr_max)  corr_max = corr_temp;
			}
		
			if (corr_max >  mysets[s].info_bit[nbit]){
				 mysets[s].corr_info[nbit] = corr_max ;
			} else{
				 mysets[s].corr_info[nbit] =  mysets[s].info_bit[nbit] ;
			}
			// calculate the total correlation information leakage in the set
			mysets[s].c_total_info+=mysets[s].corr_info[nbit];
		}

	}
	return;
}


void print_results (FILE *fp,FILE * fp2,struct sets *mysets, int scheme, int set_lines){
	//printf("------------------------------\n");
	fprintf(fp,"Number of addresses read : %lld\n", a_number);
	fprintf(fp, "Memory footprint is : %lld\n", (max_addr-min_addr)*64);
        fprintf(fp,"Statistics results using");
	fprintf(fp2,"Statistics results using");

        switch (scheme){
		case 0:
		    fprintf(fp," Module Scheme \n");
		    fprintf(fp2," Module Scheme \n");
		break;
		case 1:
                    fprintf(fp," Rotation Right by 3 bits \n");
		    fprintf(fp2," Rotation Right by 3 bits \n");
                break;
		case 2:
                    fprintf(fp," XOR Scheme \n");
	            fprintf(fp2," XOR Scheme \n");		
                break;
		case 3:
                    fprintf(fp," Rotate Right by 1 & XOR  \n");
		    fprintf(fp2," Rotate Right by 1 & XOR  \n");
                break;
		case 4:
                    fprintf(fp," Square TAG ( select midle part) XOR with set\n");
		    fprintf(fp2," Square TAG ( select midle part) XOR with set\n");
                break;
		case 5:
                    fprintf(fp," Odd multiplier case 1 (by 7) \n");
                    fprintf(fp2," Odd multiplier case 1 (by 7) \n");
                break;
		case 6:
                    fprintf(fp," Intel Slide Cache \n");
                    fprintf(fp2," Intel Slide Cache  \n");
                break;
		case 7:
                    fprintf(fp," DES (Similar to CEASER) \n");
                    fprintf(fp2," DES (Similar to CEASER) \n");
                break;

		case 8:
                    fprintf(fp," CEASER Implementaqtion \n");
                    fprintf(fp2," CEASER Implementaqtion \n");
                break;

		case 9:
		    fprintf(fp," TAG PERMUTATION and later XOR SET \n");
                    fprintf(fp2," TAG PERMUTATION and later XOR SET \n");

/*
		case 10:
                    fprintf(fp," SHA1 (select set bits only)  XOR with set \n");
                    fprintf(fp2," SHA1 (select set bits only)  XOR with set \n");
                break;
*/
	}

//	printf("------------------------------\n");
	int s=0, nbit=0;
	uint64_t elements_sum=0;
        double  entropy_average=0,  leak_average=0;
        double  avg_heavy_leak=0, total_avg_heavy_leak=0 ; 
        double leak_average_set_index=0, leak_average_tag = 0;
        double avg_cor_heavy_leak;
        double  leak_cor_average;
	int set_bits = log2(set_lines);
	int set_lines_used =0;

	// print Set, elements in the set and final leak

        fprintf(fp,"Set,Elements,Entropy,Info leakage, Set Info Leakage, Tag Info Leakage, Avg heavy info leak, Correlation Info leakage,  Avg heavy corr info leak\n");
	// only to calculate the  elements_sum
	for(s=0; s<set_lines ;s++){
		 elements_sum += mysets[s].elements;
		 if (mysets[s].elements !=0){
                      set_lines_used++ ;
		 }
	}
	double set_fraction;
        for(s=0; s<set_lines ;s++){
		//printf("Set: %d\tAcceses: %ld    \tEntropy: %f    \t Info leakage: %f  \n",s, mysets[s].elements ,mysets[s].info, 42 - mysets[s].info );
		avg_heavy_leak =  mysets[s].elements * (42 - mysets[s].info)/elements_sum; 
                avg_cor_heavy_leak =  mysets[s].elements *  mysets[s].c_total_info /elements_sum;
		//fprintf(fp,"%d,%ld,%f,%f,%f,%f,%f\n",s, mysets[s].elements ,mysets[s].info, 42 - mysets[s].info,avg_heavy_leak, mysets[s].c_total_info,avg_cor_heavy_leak);
		fprintf(fp,"%ld,%ld,%f,%f,%f,%f,%f,%f,%f\n",s, mysets[s].elements ,mysets[s].info, 42 - mysets[s].info, set_bits - mysets[s].set_index_info, 42 - set_bits - mysets[s].tag_info ,avg_heavy_leak, mysets[s].c_total_info, avg_cor_heavy_leak);
				

                set_fraction = (double) mysets[s].elements/(double)elements_sum;
		entropy_average += mysets[s].info*set_fraction;
		leak_average += (42 - mysets[s].info)*set_fraction;
		leak_average_set_index += (set_bits - mysets[s].set_index_info)*set_fraction;
		leak_average_tag += ( (42-set_bits) - mysets[s].tag_info)*set_fraction;
                leak_cor_average +=  mysets[s].c_total_info *set_fraction;
                total_avg_heavy_leak += avg_heavy_leak;
	}
//	fprintf (fp,"Total Addresses: %lld ,Average Entropy: %f ,Average Info Leakage: %f ,Total Avg Heavy Leak: %f\n",elements_sum,entropy_average/set_lines,leak_average/set_lines, total_avg_heavy_leak);
//	fprintf (fp,"Total Addresses:,%lld,Average Entropy:,%f,Average Info Leakage,%f, Set Index Leakage, %f, Tag Info Leakage, %f, Total Avg Heavy Leak:,%f\n",elements_sum,entropy_average,leak_average, leak_average_set_index, leak_average_tag, total_avg_heavy_leak);
	fprintf (fp,"Total Addresses:,%lld,Average Entropy:,%f,Average Info Leakage,%f, Set Index Leakage, %f, Tag Info Leakage, %f, Total Avg Heavy Leak:,%f, Correlation Leak Avg, %f\n",elements_sum,entropy_average,leak_average, leak_average_set_index, leak_average_tag, total_avg_heavy_leak, leak_cor_average);
	
	fprintf (fp,"Set lines used:, %d , Utilization:, %f \n", set_lines_used, ((float)set_lines_used/set_lines)  );	

	// print final Ep[i] for s =11

       	//s =11;
//	int set_lines_used =0;
        fprintf(fp2,"s,bit_pos,probability\n");
	for (s=0; s<set_lines; s++){
//		if (mysets[s].elements !=0) 
//			set_lines_used++ ;
        	for (nbit=0;nbit<42; nbit++){
			if (mysets[s].elements ==0){
                                mysets[s].p1[nbit] =0;
                        } else {
                                mysets[s].p1[nbit]=  (double) mysets[s].num_1[nbit]/(double) mysets[s].elements;
                        }
                 	//printf("For s = %d  p1[%d] = %f \n", s, nbit, mysets[s].p1[nbit]);
		 	//fprintf(fp2,"%d,%d,%f\n", s, nbit, mysets[s].p1[nbit]);
			fprintf(fp2,"%d,%d,%f,%f\n", s, nbit, mysets[s].p1[nbit],mysets[s].corr_info[nbit]);

        	}
	}
//	fprintf(fp2,"Set lines used:, %d , %Utilization:, %f \n", s, set_lines_used, (set_lines_used*100)/set_lines );
	return;
}


char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

int16_t classify_miss(cache *cash, memref *mr) {
	return 0;
}


//	clean_all		cleans all of the dirty lines in a cache
void	clean_all(cache *cash, int initial_way, int final_way) {
	cacheline       *victim;                //      cache line evicted for a cache miss
        cacheset        *set;                   //      pointer to matching set for input address		
	int64_t         crnt_time;              //      current time of recent action
	int             main_mem;               //      flag set to 1 when cash is main memory pseudo cache
	int8_t          segment;                //      memory segment code from input
	int i;
	int way;

	for (i=0 ; i < cash->nmbr_sets ; i++){

		set  = &cash->sets[i];

		// start with the first cache line of the set 
	
		victim = set->lru;

//		while ( victim != NULL){
//		for (way = 0; way < cash->assoc; way++) {       //      test all lines in the set
		for (way = initial_way; way < final_way; way++) {       //      test all lines in the set
			if (victim->valid != 0  &&  victim->dirty != 0) {                       //      write-back is needed?
        	                victim->oper = MRWRITE;
				main_mem = (cash->lower == NULL);     				// Not write back if we are in the last memory 
				if (!main_mem) {			
	                       		crnt_time = reference(cash->lower, NULL, victim);               //      write back data and mark time
	        	                cash->last_busy = crnt_time;                                    //  mark cache as busy during write back
				}
                	        set->wrback[segment]++;                                         //      increment write back counter
	        	}
            	        victim->valid =0;
			victim->dirty = 0;
			victim = victim->mru;
		}
	}
	// reset the DES password
	// generate a new dea_key
        generate_rand(&des_key);
         //reset_DES = 1;

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


        // only change the set in the share cache
//	if (cash->level == 3){   // level 3 is shared cache
//		printf("cache level: %d\n",cash->level);
		int32_t pid = 0;
		int32_t asid = 0;
//		if (mr) {
//			pid = mr->pid;
//			asid = mr->asid;
//		}
//		printf("ASID: %d\n",asid);
//		printf("Pid: %d\n", pid);
//		printf("Address: %p\n",adrs_in);
//		printf("Level: %d\n", cash->level);
		// calculation of setmask for the tag in the xor operation
		int result = 1;
		int  exponent = cash->log2blksize ;
		while (exponent != 0) {
			result *= 2;
			--exponent;
		}
//		printf("adress: %p, tagadrs: %p, log2sbsize: %p, log2size: %p, Masked: %p, set %d \n", ((int64_t) adrs_in) , tagadrs, cash->log2sbsize, cash->log2size, (tagadrs >> (cash->log2blksize + cash->log2sbsize)) & (result-1), set_nmbr );
//		printf("Set #: %p\n", set_nmbr);
//		int32_t tag_nmbr = (tagadrs >> (cash->log2blksize + cash->log2sbsize));
//		int32_t mask = ((1 << asid) - 1);
//		printf("%X Log2: %X vs popcnt: %X\n", cash->setmask, (int)log2(cash->nmbr_sets), __builtin_popcount(cash->setmask));
//		int32_t lower_tag = ((1 << asid) - 1) & tag_nmbr;
//		int32_t lower_set = ((1 << asid) - 1) & set_nmbr;
//		printf("Tag #: %p. Masked tag: %p. Masked set: %p\n", tag_nmbr, lower_tag, lower_set);
//		printf("New tag: %p. New set: %p\n", (tag_nmbr & ~mask) | lower_set, (set_nmbr >> asid) | (lower_tag << ((int)log2(cash->nmbr_sets) - asid)));
//		tag_nmbr = (tag_nmbr & ~mask) | lower_set;
//		set_nmbr = (set_nmbr >> asid) | (lower_tag << ((int)log2(cash->nmbr_sets) - asid));
/*	
		if (pid == 0) {
			// Nothing
		} else if (pid == 1) {
			// XOR
			// set_nmbr |= tag_nmbr & ((1 << (int)log2(cash->nmbr_sets)) - 1);
		} else if (pid == 2) {
			// AND
			// set_nmbr = tag_nmbr & ((1 << (int)log2(cash->nmbr_sets)) - 1) & set_nmbr;
		} else if (pid == 3) {
			// OR
			// set_nmbr = tag_nmbr & ((1 << (int)log2(cash->nmbr_sets)) - 1) | set_nmbr;
		} else {
			printf("Unsupported PID\n");
		}
*/
//		set_nmbr = tag_nmbr & (result-1) ^ set_nmbr;
//	        printf("Tag Address (setmask): %llx  %ld  %ld\n", tagadrs, tagadrs >> (cash->log2blksize + cash->log2sbsize),
//			set_nmbr) ;
//	}
//	printf("Before: %u, %u, %u\n", set_nmbr, (unsigned)log2(cash->nmbr_sets), pid);
  //	set_nmbr = rotl(set_nmbr, (int)log2(cash->nmbr_sets), pid);
//	printf("After: %d\n", set_nmbr);



// print to a CVS file the address and the set number
// address = tagadrs >> (cash->log2blksize + cash->log2sbsize
// set number = set_nmbr

		int s;
		uint64_t a = adrs_in  >> 6;  // IMPORTANT : eliminate the offset part
		if (a < min_addr) { min_addr = a; }
	        if (a > max_addr) { max_addr = a; }


	if ( (cash->ins_or_data == 1) && (cash->level==cache_test) &&  (cache_test >0)  ){    // Only save data and print of level of the select cache
		a_number++;
		set__lines= cash->nmbr_sets;
		set_nmbr   = compute_set(a,scheme, cash->nmbr_sets);	

		//calculate Ps
		estimatePs(&mysets[set_nmbr],a);

		set = &cash->sets[set_nmbr];
                set->access[segment]++;         //      increment access counter
 

		//introduce the counter 
		counter_reflash++;
		counter_dynamic++;
                
        // Flash the chache if counter_reflash == cache_flash
        if ( (counter_reflash == cache_flash) &&  cache_flash > 0){
		    //int initial_way=0; 
            clean_all (cash,0, cash->assoc);
			counter_reflash =0;
			counter ++;
            if ( scheme == 7){  // only in case of use DES scheme
				// generate a new dea_key
				generate_rand(&des_key);
			}
		}
		
		// Dynamic modifications of the chache's ways if counter_dynamic == dynamic_flash
		// steps of actual_ways -2 or +2 
		if ( (counter_dynamic == dynamic_flash) &&  dynamic_flash > 0){
			counter_dynamic =0;
			counter ++;
			// if (cash->actual_way ==  (0.5 *cash->assoc) ){  // increasing from lower 
			if (cash->actual_way ==  (2) ){  // increasing from 2
				increase_way=1;
				// increase the ways
				cash->actual_way = cash->actual_way +2;
				// not flashing cache
			} else if (cash->actual_way == cash->assoc ){  // decreasing from top 
				increase_way=0;
				
				// Flash two last ways of the cash
				clean_all (cash,cash->actual_way -2, cash->actual_way);
				// reduce the ways
				cash->actual_way = cash->actual_way -2;
			} else {  // intermediate state -> increasing o decreasing
				
				if (increase_way==0){  // decreasing
					// Flash two last ways of the cash
					clean_all (cash,cash->actual_way -2, cash->actual_way);
					// reduce the ways
					cash->actual_way = cash->actual_way -2;	
				} else { 				// incrementing 
					cash->actual_way = cash->actual_way +2;	
				}
			}
		}
		

/*      //  step to total and 50% ways changing
		if ( (counter_dynamic == dynamic_flash) &&  dynamic_flash > 0){
			counter_dynamic =0;
			counter ++;
			if  (cash->actual_way == cash->assoc ){
				// Flash two last ways of the cash
				clean_all (cash,cash->assoc/2, cash->actual_way);
				// reduce the ways
				cash->actual_way = cash->assoc/2;
			} else {
				// increase the ways
				cash->actual_way = cash->assoc;
				// not flashing cache
			}
		}
		
*/		
/*	 //  step to total to 2 ways and keep until total execution
		if ( (counter_dynamic == dynamic_flash) &&  (dynamic_flash > 0) ){
			// make sure K1D

			//counter_dynamic =0;
			counter ++;
			// Flash two last ways of the cash
			//clean_all (cash,cash->assoc/2, cash->actual_way);
			clean_all (cash,2, cash->actual_way);
			// reduce the ways
			//cash->actual_way = cash->actual_way -2;
			//printf("Reduce the associativity from %d to %d \n", cash->actual_way, 2);
			cash->actual_way =  2;
		}
*/

	} else  {    //  Rest of the caches

	      set = &cash->sets[set_nmbr];
              set->access[segment]++;         //      increment access counter
	}

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
			//set->evict[segment]++;									//	increment evict counter
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
	
	int associativity;
	int dynamic_assoc =0 ;
	switch (dynamic_assoc){
		case 0:
			//associativity = cash->assoc;
			associativity = cash->actual_way;
			break;
		case 1:
			//associativity = cash->assoc - 2;
			associativity = cash->actual_way - 2;
			break;
		case 2:
			//associativity = cash->assoc - 4;
			associativity = cash->actual_way - 4;
			break;
	}
	
//	for (way = 0; way < cash->assoc; way++) {	//	test all lines in the set
//	if ((cash->ins_or_data == 1) && (cash->level  == cache_test)) printf("Assoc: %d and %d\n", associativity, cash->actual_way);
	for (way = 0; way < associativity; way++) {   //	test all lines in the set
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

