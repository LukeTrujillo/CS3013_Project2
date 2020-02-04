/*
 *	main.c
 *	Authors: Luke Trujillo, Mike Capobianco
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHYSICAL_MEMORY_SIZE 64
#define PAGE_SIZE 16
#define VIRTUAL_ADDRESS_SPACE 64

#define VIRTUAL_ADDRESS_LENGTH 6
#define PAGE_NUMBER (PHYSICAL_MEMORY_SIZE / PAGE_SIZE)

#define OFFSET_SIZE 4
#define PAGE_TABLE_ENTRY_SIZE 1
#define PAGE_TABLE_BASE_REGISTER 0

#define PFN_SHIFT 0

#define MAX_PROCESS 4

//two left-most bits are VPN, the rest are the offset

unsigned char memory[MAX_PROCESS][PHYSICAL_MEMORY_SIZE];


unsigned int const VPN_MASK = 0b110000;
unsigned int const OFFSET_MASK = 0b001111;



int main(int argc, char** argv) {

	//parse stuff here


	char command[100];
	strcpy(command, argv[2]);

	unsigned int process_id = atoi(argv[1]);
	unsigned int virtual_address = atoi(argv[3]);

	unsigned int value = atoi(argv[4]);

	if(strcmp(command, "map") == 0) {

		
	} 

	unsigned int vpn = virtual_address & VPN_MASK;
	vpn = vpn >> OFFSET_SIZE;

	unsigned int page_table_entry = (vpn * PAGE_TABLE_ENTRY_SIZE) + PAGE_TABLE_BASE_REGISTER;
	//first 16 bytes (or 1st page) is the page_table_thing

	//5 pages for each process

	unsigned int pfn = memory[vpn];


	unsigned int offset = virtual_address & OFFSET_MASK;

	unsigned int physical_address = (pfn << PFN_SHIFT) | offset;


	/*
		page frame 0 is for the page table
		
		we need to store the process_id
		and return the page frame number
	*/
	


	//convert the number to binary 
	// isolate the two leftmost bits 
	
	
	
	printf("VA: %i VPN: %i OFFSET: %i PFN: %i PA: %i \n", virtual_address, vpn, offset, pfn, physical_address);

	return 0;
}

