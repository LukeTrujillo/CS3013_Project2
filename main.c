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

#define SWAP_SPACES 64

//two left-most bits are VPN, the rest are the offset
char memory[PHYSICAL_MEMORY_SIZE];
int hardwareRegister[MAX_PROCESS];

unsigned int pageTaken[PAGE_NUMBER];
unsigned int swapSlotTaken[SWAP_SPACES];

unsigned int getPFN(unsigned int, unsigned int);

void map(char, char, char);
void addPTE(char, char, char);

void createPageTable(unsigned int);
void setPTAddress(unsigned int, char, unsigned int);

unsigned int hasPageTable(unsigned int);
char getFlagByte(unsigned int, unsigned int);
unsigned int isWritable(unsigned int, unsigned int);

void setFlagByte(unsigned int, unsigned int, int, int, int);

void setup();
void parse(char*, char**);


char freePage();
void updatePTEOnDisk(unsigned int, unsigned int);
char bringToDisk(unsigned int);
char getPTAddress(unsigned int);
char* loadPageTable(unsigned int);
unsigned int hasPageTable(unsigned int);

unsigned int vpnExists(unsigned int, unsigned int);
unsigned int isValid (unsigned int, unsigned int);

char const HR_ADDR_MASK = 0b1111110;
char const HR_LOC_MASK = 0b1;


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
			
			virtual_address = (virtual_address & 0b110000) >> 4;

			if(vpnExists(process_id, virtual_address)) {
				
				printf("page exists\n");
				
				if(isWritable(process_id, virtual_address) == value) {
						printf("Error: virtual page %d is already mapped with bit rw_bit = %d\n", virtual_address, value);
				} else {
						setFlagByte(process_id, virtual_address, -1, -1, value);
						printf("Updating permissions for page %d (frame %d)\n", virtual_address, getPFN(process_id, virtual_address));
				}
			} else {
				map(process_id, virtual_address, value);
			}

		} else if(strcmp(command, "store") == 0) {
			 int vpn = (virtual_address & 0b110000) >> 4;
			
			
			if(isWritable(process_id, vpn)) {
				unsigned int pfn = getPFN(process_id, vpn);
				

				unsigned int offset = virtual_address & 0b1111;
				unsigned int physical_address = (PAGE_SIZE * pfn) + offset;

				memory[physical_address] = value;
				
				
				printf("Stored value %d at virtual address %d (physical address %d)\n", value, virtual_address, physical_address);
				
			} else {
				printf("Error: writes are not allowed to this page\n");
			}
		} else if(strcmp(command, "load") == 0) {
			
		} 
	}

	return 0;
}

void setup() {
	
	for(int x = 0; x < MAX_PROCESS; x++) {
		hardwareRegister[x] = -1;
	}

	for(int x = 0; x < PAGE_NUMBER; x++) {
		pageTaken[x] = 1;
	}

	for(int x = 0; x < PAGE_NUMBER; x++) {
		swapSlotTaken[x] = 1;
	}

}
void map(char process_id, char vpn, char rwFlag) {
	int shouldCreate = !hasPageTable(process_id);

	if(shouldCreate) {
		createPageTable(process_id); //create the page table
	}

	addPTE(process_id, vpn, rwFlag); //otherwise just add and allocate the space
	
}
void addPTE(char process_id, char vpn, char rwFlag) {
	char *pageTable = loadPageTable(process_id);

	char page = freePage();

	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE] = page;
	
	setFlagByte(process_id, vpn, 1, 1, rwFlag);
	
	printf("Mapped virtual address %d into physical frame %d\n", vpn, page);
}
void createPageTable(unsigned int process_id) {

	printf("PID %d has no page table, making one...\n", process_id); 
	unsigned int makeLocation = freePage(); //will either swap the page or return a free one

	setPTAddress(process_id, makeLocation, 1);

}

/* 
 1 = on physical memeory, 0 = disk
*/
void setPTAddress(unsigned int process_id, char address, unsigned int location) {
	hardwareRegister[process_id] = (address << 1) | (location & 0b1);
}

/*
	
*/
char freePage() {
	printf("Request for a free page has been made!\n");

	//check if there is already a free page, if there is return the pfn
	for(int x = 0; x < PAGE_NUMBER; x++) {
		if(pageTaken[x] == 1) { 
			printf("Page %d is free.\n", x);
			pageTaken[x] = 0;
			return x; 
		}
	}

	//choose who to swap
	srand(time(0));

	int evictee = rand() % PAGE_NUMBER; //the page number to be evicted
	
	//swap out
	char diskAddress = bringToDisk(evictee);

	printf("Evicting page %d to swap space %d\n", evictee, diskAddress);

	updatePTEOnDisk(evictee, diskAddress); //find the PTE, add the disk address, and change the flag

	return freePage();
}
char* getPageFrame(unsigned int pfn) {
	return &memory[pfn * PAGE_SIZE];
}
void updatePTEOnDisk(unsigned int evictee, unsigned int diskAddress) {
	for(int x = 0; x < MAX_PROCESS; x++) {
		char reg = hardwareRegister[x];
	
		if((reg & HR_ADDR_MASK) == evictee && (reg & HR_LOC_MASK)) {
			//is a PTE

			char new_reg = (diskAddress << 1) | 0; //set the address and note that it is not on physical memeory
			hardwareRegister[x] = new_reg;
		}
	}

	//go to each page table and find the one being evicted

	for(int x = 0; x < MAX_PROCESS; x++) {
		char *pageTable = loadPageTable(x);

		for(int y = 0; y < PAGE_SIZE; y += PAGE_TABLE_ENTRY_SIZE) {
			if(pageTable[y] == evictee && pageTable[y + 3] & HR_LOC_MASK) {
				
				printf("PFN %d swapped into swap space %d\n", evictee, diskAddress);

				pageTable[y + 1] = diskAddress; 
				pageTable[y + 3] = pageTable[y + 3] & ~(HR_LOC_MASK);

			}
		}
	}
	

	//not a page table	
}
char bringToDisk(unsigned int evictee) {

	char diskAddress;
	//find where to swap it too
	for(int x = 0; x < SWAP_SPACES; x++) {
		if(swapSlotTaken[x] == 1) {
			diskAddress = x;
			swapSlotTaken[x] = 0;
			break;
		}
	}

	char *page = getPageFrame(evictee);

	FILE *file;
	file = fopen("swapspace.bin", "rb+");

	fseek(file, diskAddress, SEEK_SET);

	fwrite(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

	fclose(file);

	pageTaken[evictee] = 0;

	for(int x = evictee * PAGE_SIZE; x < (evictee + 1) * PAGE_SIZE; x++) {
		memory[x] = 0;
	}

	return diskAddress;
}
void bringFromDisk(unsigned int diskAddress) { //need to update PT
	unsigned int destination = freePage();
	
	char page[PAGE_SIZE];
	
	FILE *file;
	file = fopen("swapspace.bin", "rb+");

	fseek(file, diskAddress, SEEK_SET);

	fread(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

	fclose(file);

	swapSlotTaken[diskAddress] = 1;

	char *putIn = getPageFrame(destination);

	for(int x = 0; x < PAGE_SIZE; x++) {
		putIn[x] = page[x]; //copy things over 
	}

	

}
char getPTAddress(unsigned int process_id) { return (hardwareRegister[process_id] >> 1); }
char* loadPageTable(unsigned int process_id) {
	if(!(hardwareRegister[process_id] & HR_LOC_MASK)) { //if the page table is not loaded 
		printf("need to load page table from memory\n");
		bringFromDisk(getPTAddress(process_id));
	} 

	//now its on the disk

	char *pageTable;

	pageTable = &memory[getPTAddress(process_id) * PAGE_SIZE];

	return pageTable;
}
unsigned int hasPageTable(unsigned int process_id) { return hardwareRegister[process_id] != -1; }
void parse(char* str, char** strs){
	strs[0] = strtok(str, ",");
	strs[1] = strtok(NULL, ",");
	strs[2] = strtok(NULL, ",");
	strs[3] = strtok(NULL, ",");
}
unsigned int getPFN(unsigned int process_id, unsigned int vpn) {
	char *pageTable = loadPageTable(process_id);
	
	return pageTable[vpn * PAGE_TABLE_ENTRY_SIZE];
}
char getFlagByte(unsigned int process_id, unsigned int vpn) {
	char *pageTable = loadPageTable(process_id);
	
	return pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 3];
}
void setFlagByte(unsigned int process_id, unsigned int vpn, int valid, int on, int rw) {
	char flagByte = getFlagByte(process_id, vpn);
	
	if(valid != -1) {
			char mask = 0b100;
			
			if(valid) {
					flagByte = flagByte | mask;
			} else {
					flagByte = flagByte & ~mask;
			}
	}
	
	if(on != -1) {
			char mask = 0b10;
			if(on) {
					flagByte = flagByte | mask;
			} else {
					flagByte = flagByte & ~mask;
			}
	}
	
	if(rw != -1) {
			char mask = 1;
			if(rw) {
					flagByte = flagByte | mask;
			} else {
					flagByte = flagByte & ~mask;
			}
	}
	
	char *pageTable = loadPageTable(process_id);
	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 3] = flagByte;
	
	printf("Flag byte updated %d RW: %d V: %d\n", flagByte, rw, valid);
	
}
unsigned int isWritable(unsigned int process_id, unsigned int vpn) {
	char flagByte = getFlagByte(process_id, vpn);
	
	printf("isWritable flagByte: %d\n", (flagByte & 1));
	return (flagByte & 0b1);
	
}

unsigned int vpnExists(unsigned int process_id, unsigned int vpn) {
	return hardwareRegister[process_id] != -1 && isValid(process_id, vpn);
}

unsigned int isValid(unsigned int process_id, unsigned int vpn) {
	return (getFlagByte(process_id, vpn) & 0b100) != 0;
}
