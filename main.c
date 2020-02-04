/*
 *	main.c
 *	Authors: Luke Trujillo, Mike Capobianco
 */

#include <stdio.h>
#include <stdlib.h>

#define PHYSICAL_MEMORY_SIZE 64
#define PAGE_SIZE 16
#define VIRTUAL_ADDRESS_SPACE 64

#define VIRTUAL_ADDRESS_LENGTH 6
#define PAGE_NUMBER (PHYSICAL_MEMORY_SIZE / PAGE_SIZE)

#define OFFSET_SIZE 4
//two left-most bits are VPN, the rest are the offset

unsigned char memory[PHYSICAL_MEMORY_SIZE];


unsigned int const VPN_MASK = 0b110000;
unsigned int const OFFSET_MASK = 0b001111;


int main(int argc, char** argv) {

	unsigned int virtual_address = atoi(argv[1]);


	unsigned int vpn = virtual_address & VPN_MASK;
	vpn = vpn >> OFFSET_SIZE;

	unsigned int offset = virtual_address & OFFSET_MASK;

	unsigned int physical_address = PAGE_SIZE * vpn + offset;

	


	//convert the number to binary 
	// isolate the two leftmost bits 
	
	
	
	printf("VA: %i VPN: %i OFFSET: %i PA: %i \n", virtual_address, vpn, offset, physical_address);

	return 0;
}
