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
#define PAGE_TABLE_ENTRY_SIZE 4
#define PAGE_TABLE_BASE_REGISTER 0

#define PFN_SHIFT 0

#define MAX_PROCESS 4

//two left-most bits are VPN, the rest are the offset

unsigned char memory[MAX_PROCESS * PHYSICAL_MEMORY_SIZE];
int hardwareRegister[MAX_PROCESS];

unsigned int freeList[MAX_PROCESS];

unsigned int processFreeList[MAX_PROCESS][PAGE_NUMBER];

unsigned char swapMemory[MAX_PROCESS][PAGE_SIZE];


unsigned int const VPN_MASK = 0b110000;
unsigned int const OFFSET_MASK = 0b001111;

void setup();
char* getNextFreePage(int, int, int);
int getPageTableBaseRegister(int);


void addPageTableEntry(int, int, int);
int convertVPNtoPFN(int, int);
int main(int argc, char** argv) {

	setup();

	//parse stuff here
	unsigned int process_id = atoi(strtok(argv[1], ","));
	char *instruction_type = strtok(NULL, ",");
	unsigned int virtual_address = atoi(strtok(NULL, ","));
	unsigned int value = atoi(strtok(NULL, ","));


	char command[100];
	strcpy(command, instruction_type);

	if(strcmp(command, "map") == 0) {

		getNextFreePage(process_id, virtual_address, value);

	} else if(strcmp(command, "store") == 0) {
		getNextFreePage(0, 0, 1);

		unsigned int vpn = virtual_address & VPN_MASK;
		vpn = vpn >> OFFSET_SIZE;

		unsigned int pfn = convertVPNtoPFN(process_id, vpn);

		
		
		unsigned int offset = virtual_address & OFFSET_MASK;

		unsigned int physical_address = (pfn | offset) + getPageTableBaseRegister(process_id) + PAGE_SIZE;

		memory[physical_address] = value;

		printf("Stored value %d at virtual address %d (physical address %d)\n", value, virtual_address, physical_address);
	

	}

	/*
		psage frame 0 is for the page table
		
		we need to store the process_id
		and return the page frame number
	*/
	


	//convert the number to binary 
	// isolate the two leftmost bits 
	
	

	return 0;
}

void setup() {
	for(int x = 0; x < MAX_PROCESS; x++) {
		freeList[x] = 1; //provide the index of each available page frame;s
	}
	
	for(int x = 0; x < MAX_PROCESS; x++) {
		hardwareRegister[x] = -1;
	}

	for(int x = 0; x < MAX_PROCESS; x++) {
		for(int y = 0; y < PAGE_NUMBER; y++) {
			processFreeList[x][y] = 1;
		}	
	}
}

int getPageTableBaseRegister(int process_id) {
	if(hardwareRegister[process_id] != -1) {
		return hardwareRegister[process_id];
	} else { //find which is a starting register


		for(int x = 0; x < MAX_PROCESS; x++) {
			if(freeList[x] == 1) {
				//found a free space
				hardwareRegister[process_id] = x * PHYSICAL_MEMORY_SIZE;

				freeList[x] = 0;

				printf("Put page table for PID %d in physical frame 0.\n", process_id);

				return hardwareRegister[process_id];
			}
		}

	}

	//something messed up
	printf("no more memory\n");
}

char* getNextFreePage(int process_id, int vpn, int rwFlag) {
	int startingIndex = getPageTableBaseRegister(process_id);
	
	//now we need to find the next free page

	addPageTableEntry(process_id, vpn, rwFlag);
	convertVPNtoPFN(process_id, vpn);



}

char pageTable[PAGE_SIZE];

char* getPageTable(int process_id) {
	int startingIndex = getPageTableBaseRegister(process_id);

	for(int x = startingIndex; x < startingIndex + PAGE_SIZE; x++) {
		pageTable[x - startingIndex] = memory[x]; //copy over the page table
	}

	return pageTable;
}

void addPageTableEntry(int process_id, int vpn, int rwFlag) {
	int startingIndex = getPageTableBaseRegister(process_id);
	
	char *pageTable = getPageTable(process_id);

	//pfn on 2 left most bytes, r/w on right most byte

	//now we need to find the next available page

	for(int x = 0; x < PAGE_NUMBER - 1; x++) {
		if(processFreeList[process_id][x] == 1) {
			processFreeList[process_id][x] = 0;

			printf("Mapped virtual address %d (page %d) into physical frame %d\n", vpn, x, ((startingIndex + x * PAGE_SIZE) / PAGE_SIZE) + 1);
			
			memory[startingIndex + vpn * PAGE_TABLE_ENTRY_SIZE] = x;

			memory[(startingIndex + PAGE_TABLE_ENTRY_SIZE - 1) + vpn * PAGE_TABLE_ENTRY_SIZE] = rwFlag + 48;
			break;
			
		}
	}	
}

int convertVPNtoPFN(int process_id, int vpn) {
	int baseRegister = getPageTableBaseRegister(process_id);

	int page_table_entry = vpn * PAGE_TABLE_ENTRY_SIZE + baseRegister;

	char *entry = &memory[page_table_entry];

	return entry[0];

	

}



