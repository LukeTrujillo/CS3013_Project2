/*
 *	main.c
 *	Authors: Luke Trujillo, Mike Capobianco
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

#define LOC_BIT_SHIFT 1
#define VALID_BIT_SHIFT 2
#define WRITE_BIT_SHIFY 0

//two left-most bits are VPN, the rest are the offset

unsigned char memory[MAX_PROCESS * PHYSICAL_MEMORY_SIZE];
int hardwareRegister[MAX_PROCESS];

unsigned int freeList[MAX_PROCESS];

unsigned int processFreeList[MAX_PROCESS][PAGE_NUMBER];

unsigned char swapMemory[MAX_PROCESS][PAGE_SIZE];

unsigned int swapBlockCounter;

unsigned int const VPN_MASK = 0b110000;
unsigned int const OFFSET_MASK = 0b001111;

char const VALID_BIT_MASK = 0b100;
char const LOC_BIT_MASK = 0b010;
char const WRITE_BIT_MASK = 0b001;


int createPageTableBaseRegister(int);
void setup();
char* getNextFreePage(int, int, int);
int getPageTableBaseRegister(int);
void parse(char*, char**);

void addPageTableEntry(int, int, int);
int convertVPNtoPFN(int, int);

void swap(int, int, int);

int pageExists(int, int);

void setFlags(unsigned int, unsigned int, int, int, int);
void setDiskAddress(unsigned int, unsigned int, char);

void insertInto(unsigned int, unsigned int, unsigned int);
char getDiskAddress(unsigned int, unsigned int);

void setPTE(unsigned int, unsigned int, char);
char getFlagByte(unsigned int, unsigned int);


unsigned int isValid(unsigned int, unsigned int);
unsigned int isWritable(unsigned int, unsigned int);
unsigned int onPhysicalMemory(unsigned int, unsigned int);

unsigned int allPagesFull(unsigned int);

char getPFN(unsigned int, unsigned int);

void printPTE(unsigned int, unsigned int);

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

				if(isWritable(process_id, virtual_address) == value) {
					printf("#Error: virtual page %d is already mapped with rw_bit=%d\n", ((virtual_address & VPN_MASK) >> OFFSET_SIZE), value);
				} else {
					setFlags(process_id, virtual_address, -1, -1, value);
					printf("The permissions have been updated.\n");
				}
			} else {
				getNextFreePage(process_id, virtual_address, value);
			}

		} else if(strcmp(command, "store") == 0) {
			
			unsigned int vpn = virtual_address & VPN_MASK;
			vpn = vpn >> OFFSET_SIZE;


			if(isWritable(process_id, vpn)) {

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
	swapBlockCounter = 0;

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

	if(allPagesFull(process_id)) { //a swap needs to happen
		
		//choose a page randomly 

		//get the pfn
		srand(time(0));

		int choice = (rand() % 3); //evict the memory stored in the 0 - 2 th spot

		//now we need to find the vpn somehow

		char *pageTable = getPageTable(process_id);


		for(int x = 0; x < PAGE_SIZE; x += PAGE_TABLE_ENTRY_SIZE) {
			int temp_vpn = x / PAGE_TABLE_ENTRY_SIZE;

			if(memory[getPageTableBaseRegister(process_id) + x * PAGE_TABLE_ENTRY_SIZE] == choice && isValid(process_id, temp_vpn) && onPhysicalMemory(process_id, temp_vpn)) {
			 	swap(process_id, temp_vpn, vpn);
			}	
		}
		
			
		addPageTableEntry(process_id, vpn, rwFlag);	


	} else {
		addPageTableEntry(process_id, vpn, rwFlag);
		convertVPNtoPFN(process_id, vpn);

	}
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


			setFlags(process_id, vpn, 1, 1, rwFlag);
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


void parse(char* str, char** strs){
	strs[0] = strtok(str, ",");
	strs[1] = strtok(NULL, ",");
	strs[2] = strtok(NULL, ",");
	strs[3] = strtok(NULL, ",");
}

int pageExists(int process_id, int vpn) {
	//check if a page exists 
	
	char valid = isValid(process_id, vpn);


	if(valid) {
		return 1;
	}
	
	return 0;
}

void swap(int process_id, int outgoing, int incoming) {
	//assumptions no space for incoming so decided to evict outgoing

	int baseRegister = getPageTableBaseRegister(process_id);

	char *outgoing_page;
	char pfn = memory[baseRegister + outgoing * PAGE_TABLE_ENTRY_SIZE];

	unsigned int out_physical_address = pfn  + getPageTableBaseRegister(process_id) + PAGE_SIZE;
	outgoing_page = &memory[out_physical_address];

	
	if(isValid(process_id, outgoing) && onPhysicalMemory(process_id, outgoing)) {
		printPTE(process_id, outgoing);
	
		FILE *file;
		file = fopen("swapspace.bin", "rb+");

		char disk_address = swapBlockCounter * PAGE_SIZE;

		fseek(file, disk_address, SEEK_SET);
	
		fwrite(outgoing_page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

		fclose(file);

		setFlags(process_id, outgoing, -1, 0, -1);

		setDiskAddress(process_id, outgoing, disk_address);		
		swapBlockCounter++; //increase the counter for next time

		//now we need to load the next one

		printPTE(process_id, outgoing);

		insertInto(process_id, incoming, convertVPNtoPFN(process_id, outgoing));

		printf("the swap should of happened\n");
		

	}
}
void printPTE(unsigned int process_id, unsigned vpn) {
	printf("VPN: %d PFN: %d d_addr: %d valid: %d in_mem: %d loc: %d\n", vpn, getPFN(process_id, vpn), getDiskAddress(process_id, vpn),isValid(process_id, vpn), onPhysicalMemory(process_id, vpn), isWritable(process_id, vpn));

	printf("PTE: ");
	printf("%d", memory[vpn * PAGE_TABLE_ENTRY_SIZE + getPageTableBaseRegister(process_id)]);
	printf("%d", memory[vpn * PAGE_TABLE_ENTRY_SIZE + getPageTableBaseRegister(process_id) + 1]);
	printf("%d", memory[vpn * PAGE_TABLE_ENTRY_SIZE + getPageTableBaseRegister(process_id) + 2]);
	printf("%d", memory[vpn * PAGE_TABLE_ENTRY_SIZE + getPageTableBaseRegister(process_id) + 3]);
	printf("\n");

}

char getFlagByte(unsigned int process_id, unsigned int vpn) {
	return memory[vpn * PAGE_TABLE_ENTRY_SIZE + (getPageTableBaseRegister(process_id) + PAGE_TABLE_ENTRY_SIZE - 1)];
}

unsigned int isValid(unsigned int process_id, unsigned int vpn) { return (getFlagByte(process_id, vpn) & VALID_BIT_MASK) != 0; }
unsigned int onPhysicalMemory(unsigned int process_id, unsigned int vpn) { return (getFlagByte(process_id, vpn) & LOC_BIT_MASK) != 0; }
unsigned int isWritable(unsigned int process_id, unsigned int vpn) { return (getFlagByte(process_id, vpn) & WRITE_BIT_MASK) != 0; }


/**
	-1 means do not alter flagf
*/
void setFlags(unsigned int process_id, unsigned int vpn, int valid, int location, int write) {
	unsigned int baseRegister = getPageTableBaseRegister(process_id);

	char flag_byte = getFlagByte(process_id, vpn);

	if(valid != -1) {
		
		if(valid == 0) {
			flag_byte = flag_byte & ~VALID_BIT_MASK;
		} else {
			flag_byte = flag_byte | VALID_BIT_MASK;
		}

	}
	
	if(location != -1) {
		
		if(location == 0) {
			flag_byte = flag_byte & ~LOC_BIT_MASK;
		} else {
			flag_byte = flag_byte | LOC_BIT_MASK;
		}

	}
	if(write != -1) {
		
		if(write == 0) {
			flag_byte = flag_byte & ~WRITE_BIT_MASK;
		} else {
			flag_byte = flag_byte | WRITE_BIT_MASK;
		}

	}


	memory[vpn * PAGE_TABLE_ENTRY_SIZE + (baseRegister + PAGE_TABLE_ENTRY_SIZE - 1)] = flag_byte;
}

void setDiskAddress(unsigned int process_id, unsigned int vpn, char diskAddress) {
	memory[vpn * PAGE_TABLE_ENTRY_SIZE + (getPageTableBaseRegister(process_id) + 2)] = diskAddress;
}
char getDiskAddress(unsigned int process_id, unsigned int vpn) {
	return memory[vpn * PAGE_TABLE_ENTRY_SIZE + (getPageTableBaseRegister(process_id) + 2)];
}
void setPFN(unsigned int process_id, unsigned int vpn, char pfn) {
	memory[getPageTableBaseRegister(process_id) + vpn * PAGE_TABLE_ENTRY_SIZE] = pfn;
}
char getPFN(unsigned int process_id, unsigned int vpn) {
	return memory[getPageTableBaseRegister(process_id) + vpn * PAGE_TABLE_ENTRY_SIZE];
}
void insertInto(unsigned int process_id, unsigned int incoming_vpn, unsigned int pfn) {
	FILE *file;
	file = fopen("swapspace.bin", "rb+");

	char load_page[16];

	fseek(file, getDiskAddress(process_id, incoming_vpn), SEEK_SET);
	fread(&load_page, sizeof(char), sizeof(char) * PAGE_SIZE, file);


	unsigned int basePoint = getPageTableBaseRegister(process_id) + PAGE_SIZE;

	for(int x = 0; x < PAGE_SIZE; x++) {
		unsigned int accessPoint = basePoint + (pfn | x);

		memory[accessPoint] = load_page[x];
	}
	
	setFlags(process_id, incoming_vpn, -1, 1, -1);
	
	setPFN(process_id, incoming_vpn, pfn);
}

unsigned int allPagesFull(unsigned int process_id) {
	for(int x = 0; x < PAGE_NUMBER; x++) {
		if(processFreeList[process_id][x] == 1) {
			return 0;	
		}
	}
	
	return 1;
}


