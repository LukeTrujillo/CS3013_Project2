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
#define RW_MASK 1
#define VALID_MASK 2
#define LOC_MASK 4

//two left-most bits are VPN, the rest are the offset

unsigned char memory[MAX_PROCESS * PHYSICAL_MEMORY_SIZE];
int hardwareRegister[MAX_PROCESS];

unsigned int freeList[MAX_PROCESS];

unsigned int processFreeList[MAX_PROCESS][PAGE_NUMBER];

unsigned char swapMemory[MAX_PROCESS][PAGE_SIZE];


unsigned int const VPN_MASK = 0b110000;
unsigned int const OFFSET_MASK = 0b001111;

int createPageTableBaseRegister(int);
void setup();
char* getNextFreePage(int, int, int);
int getPageTableBaseRegister(int);
void parse(char*, char**);

void addPageTableEntry(int, int, int);
int convertVPNtoPFN(int, int);
int writable(int, int);
void set_writable(int, int, int);

void swap(int, int, int);

int pageExists(int, int);


int main(int argc, char** argv) {
	setup();
	while(1) {
		char word[100];
		char* args[4];
		printf("Instruction? ");
		scanf("%s", word); 

		parse(word, args);

		unsigned int process_id = atoi(args[0]);
		char *instruction_type = args[1];
		unsigned int virtual_address = atoi(args[2]);
		unsigned int value = atoi(args[3]);

		char command[100];
		strcpy(command, instruction_type);

		if(strcmp(command, "map") == 0) {

			if(pageExists(process_id, virtual_address) == 1) {

				if(writable(process_id, virtual_address) == value) {
					printf("#Error: virtual page %d is already mapped with rw_bit=%d\n", ((virtual_address & VPN_MASK) >> OFFSET_SIZE), value);
				} else {
					set_writable(process_id, virtual_address, value);
				}
			} else {
				getNextFreePage(process_id, virtual_address, value);
			}

		} else if(strcmp(command, "store") == 0) {
			
			unsigned int vpn = virtual_address & VPN_MASK;
			vpn = vpn >> OFFSET_SIZE;


			if(writable(process_id, vpn)) {

				unsigned int pfn = convertVPNtoPFN(process_id, vpn);

				unsigned int offset = virtual_address & OFFSET_MASK;

				unsigned int physical_address = (pfn | offset) + getPageTableBaseRegister(process_id) + PAGE_SIZE;

				memory[physical_address] = value;

				printf("Stored value %d at virtual address %d (physical address %d)\n", value, virtual_address, physical_address);
			} else {
				printf("#Error: you tried to access a read-only page\n");
			}
	

		} else if(strcmp(command, "load") == 0) {
			unsigned int vpn = virtual_address & VPN_MASK;
			vpn = vpn >> OFFSET_SIZE;

			unsigned int pfn = convertVPNtoPFN(process_id, vpn);

			unsigned int offset = virtual_address & OFFSET_MASK;

			unsigned int physical_address = (pfn | offset) + getPageTableBaseRegister(process_id) + PAGE_SIZE;
		
			printf("The value %d is at virtual address %d (physical address %d)\n", memory[physical_address], virtual_address, physical_address);
		} else if(strcmp(command, "swap") == 0) {
			swap(process_id, virtual_address, value);
		}
	}

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
	} 

	return -1;
}
int createPageTableBaseRegister(int process_id) {
	for(int x = 0; x < MAX_PROCESS; x++) {
		if(freeList[x] == 1) {
			//found a free space
			hardwareRegister[process_id] = x * PHYSICAL_MEMORY_SIZE;

			freeList[x] = 0;

			printf("Put page table for PID %d in physical frame 0.\n", process_id);

			return getPageTableBaseRegister(process_id);
		}
	}

	printf("no more memory\n");

	return -1;

}

char* getNextFreePage(int process_id, int vpn, int rwFlag) {
	int startingIndex = createPageTableBaseRegister(process_id);
	
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

	for(int x = 0; x < PAGE_NUMBER - 1; x++) {
		if(processFreeList[process_id][x] == 1) {
			processFreeList[process_id][x] = 0;

			printf("Mapped virtual address %d (page %d) into physical frame %d\n", vpn, x, ((startingIndex + x * PAGE_SIZE) / PAGE_SIZE) + 1);
			
			memory[startingIndex + vpn * PAGE_TABLE_ENTRY_SIZE] = x;

			//last byte: 0000 0 loc,valid,rw


			memory[(startingIndex + PAGE_TABLE_ENTRY_SIZE - 1) + vpn * PAGE_TABLE_ENTRY_SIZE] = (rwFlag & RW_MASK) | (VALID_MASK) | (LOC_MASK);
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

int writable(int process_id, int vpn) {
	int baseRegister = getPageTableBaseRegister(process_id);

	int page_table_entry = vpn * PAGE_TABLE_ENTRY_SIZE + (baseRegister + PAGE_TABLE_ENTRY_SIZE  -1);

	char flag = memory[page_table_entry] & RW_MASK;
	
	return flag;
}
void set_writable(int process_id, int vpn, int value) {
	int baseRegister = getPageTableBaseRegister(process_id);

	int page_table_entry = vpn * PAGE_TABLE_ENTRY_SIZE + (baseRegister + PAGE_TABLE_ENTRY_SIZE  - 1);

	memory[page_table_entry] = memory[page_table_entry] ^ (value * RW_MASK);


	printf("Updaing permissions for virtual page %d (frame %d)\n", vpn, convertVPNtoPFN(process_id, vpn) + 1); 
}

void parse(char* str, char** strs){
	strs[0] = strtok(str, ",");
	strs[1] = strtok(NULL, ",");
	strs[2] = strtok(NULL, ",");
	strs[3] = strtok(NULL, ",");
}

int pageExists(int process_id, int vpn) {
	//check if a page exists 

	int baseRegister = getPageTableBaseRegister(process_id);

	char pfn = memory[baseRegister + vpn * PAGE_TABLE_ENTRY_SIZE];
	
	char valid = memory[vpn * PAGE_TABLE_ENTRY_SIZE + (baseRegister + PAGE_TABLE_ENTRY_SIZE - 2)];
	
	if(processFreeList[process_id][pfn] == 0 && valid != 0) {
		return 1;
	}
	
	return 0;
}

void swap(int process_id, int outgoing, int incoming) {
	//assumptions no space for incoming so decided to evict outgoing

	int baseRegister = getPageTableBaseRegister(process_id);

	char *outgoing_page;
	char pfn = memory[baseRegister + vpn * PAGE_TABLE_ENTRY_SIZE];

	char *flags = &memory[outgoing * PAGE_TABLE_ENTRY_SIZE + (baseRegister + PAGE_TABLE_ENTRY_SIZE - 1];

	unsigned int out_physical_address = pfn  + getPageTableBaseRegister(process_id) + PAGE_SIZE;
	outgoing_page = &memory[out_physical_address];

	
	if((flags >> 1) == 3) {
		FILE *file;
		file = fopen("swapspace.bin", "rb+");

		//ppvv * PAGE_SIZE
		char disk_address = ((process_id << 2) | outgoing) * PAGE_SIZE;


		fseek(file, disk_address, SEEK_SET);
	
		fwrite(outgoing_page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

		fclose(file);

		flags = flags & (~LOC_MASK); //turns off the loc mask

		memory[outgoing * PAGE_TABLE_ENTRY_SIZE + (baseRegister + PAGE_TABLE_ENTRY_SIZE - 2] = disk_address;

		//now we need to load the next one
	}

	file = fopen("swapspace.bin", "rb+");

	char cool_stuff[PAGE_SIZE];

	fseek(file, disk_address, SEEK_SET);

	fread(&cool_stuff, sizeof(char), sizeof(char) * PAGE_SIZE, file);


	for(int x = 0; x < PAGE_SIZE; x++) {
		cool_stuff[x] = cool_stuff[x] & 255;
	}

}



