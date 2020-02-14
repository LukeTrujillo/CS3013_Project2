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


char freePage(unsigned int);
void updatePTEOnDisk(unsigned int, unsigned int);
char bringToDisk (unsigned int);
char getPTAddress(unsigned int);
char* loadPageTable(unsigned int);
unsigned int hasPageTable(unsigned int);

unsigned int vpnExists(unsigned int, unsigned int);
unsigned int isValid (unsigned int, unsigned int);
unsigned int onPhysicalMemory(unsigned int, unsigned int);

unsigned int bringFromDisk(unsigned int, unsigned int);

void updatePTEFromDisk(unsigned int, unsigned int, unsigned int, unsigned int);

char const HR_ADDR_MASK = 0b1111110;
char const HR_LOC_MASK = 0b1;


int main(int argc, char** argv) {
	setup();
	while(1) {
		char word[100];
		char* args[4];
		printf("\n\n\nInstruction? ");
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
			
				if(isWritable(process_id, virtual_address) == value) {
						printf("Error: virtual page %d is already mapped with bit rw_bit = %d\n", virtual_address, value);
				} else {
						setFlagByte(process_id, virtual_address, -1, -1, value);
						printf("Updating permissions for page %d (frame %d)\n", virtual_address, getPFN(process_id, virtual_address));
				}
			} else {
				map(process_id, virtual_address, value);
			}
			continue;
		}
		
		
		//make sure the vpn is loaded 
		int vpn = (virtual_address & 0b110000) >> 4;
			
		if(!onPhysicalMemory(process_id, vpn)) { //load this shit
			
			printf("the page needs to be loaded\n");
			
			//load from the file 
			char *pageTable = loadPageTable(process_id);
			
			char diskAddress = pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 1];
			
			unsigned int pfn = bringFromDisk(diskAddress, process_id);
			updatePTEFromDisk(process_id, vpn, pfn, diskAddress);
		}
				
		
		 if(strcmp(command, "store") == 0) {
			
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
				//is on disk
			
				unsigned int pfn = getPFN(process_id, vpn);
				
				unsigned int offset = virtual_address & 0b1111;
				unsigned int physical_address = (PAGE_SIZE * pfn) + offset;
			
				unsigned char val = memory[physical_address];
			
			
				printf("The value %d is virtual address %d (physical address %d)\n",val, virtual_address, physical_address);
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

	char page = freePage(process_id);

	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE] = page;
	
	setFlagByte(process_id, vpn, 1, 1, rwFlag);
	
	printf("Mapped virtual address %d into physical frame %d\n", vpn, page);
}
void createPageTable(unsigned int process_id) {

	printf("PID %d has no page table, making one...\n", process_id); 
	unsigned int makeLocation = freePage(process_id); //will either swap the page or return a free one

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
char freePage(unsigned int process_id) {

	//check if there is already a free page, if there is return the pfn
	for(int x = 0; x < PAGE_NUMBER; x++) {
		if(pageTaken[x] == 1) { 
			pageTaken[x] = 0;
			return x; 
		}
	}

	//choose who to swap
	srand(time(0));


		
	int evictee = rand() % PAGE_NUMBER; //the page number to be evicted
	
	while(evictee == getPTAddress(process_id)) {
		
		printf("tried to evict the calling process PT, retrying\n");
		evictee = rand() % PAGE_NUMBER;
	}
	
	
	//swap out
	char diskAddress = bringToDisk(evictee);

	printf("Evicting page %d to swap space %d\n", evictee, diskAddress);

	updatePTEOnDisk(evictee, diskAddress); //find the PTE, add the disk address, and change the flag

	return evictee;
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
			
			return;
		}
	}

	//go to each page table and find the one being evicted

	for(int x = 0; x < MAX_PROCESS; x++) {
		char *pageTable = loadPageTable(x);

		for(int y = 0; y < PAGE_SIZE; y += PAGE_TABLE_ENTRY_SIZE) {
			if(pageTable[y] == evictee && pageTable[y + 3] & HR_LOC_MASK) {
				
				printf("PFN %d swapped into swap space %d\n", evictee, diskAddress);

				pageTable[y + 1] = diskAddress; 
				setFlagByte(x, evictee, -1, 0, -1);

			}
		}
	}
	

	//not a page table	
}
void updatePTEFromDisk(unsigned int process_id, unsigned int vpn, unsigned int pfn, unsigned int diskAddress) {
	char *pageTable = loadPageTable(process_id);
	
	char *pageTableEntry = &pageTable[PAGE_TABLE_ENTRY_SIZE * vpn];
	
	char assigned = (diskAddress << 1) | 0;
	
	for(int x = 0; x < MAX_PROCESS; x++) {
		if(assigned == hardwareRegister[x]) { //is a page table
				hardwareRegister[x] = (pfn << 1) | 1;
				pageTaken[pfn] = 0;
				return;
		}
	}
	
	pageTableEntry[0] = pfn; //set the pfn
	setFlagByte(process_id, vpn, -1, 1, -1);

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
	file = fopen("swapspace.txt", "w+");

	fseek(file, diskAddress, SEEK_SET);

	fwrite(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

	fclose(file);

	pageTaken[evictee] = 1;

	for(int x = evictee * PAGE_SIZE; x < (evictee + 1) * PAGE_SIZE; x++) {
		memory[x] = 0;
	}

	return diskAddress;
}
unsigned int evict(unsigned int process_id) {
	
	char ptAddress = getPTAddress(process_id); //can not evict this
	
	srand(time(0));
	unsigned int evictee = rand() % PAGE_NUMBER; //the page number to be evicted
	
	while((evictee = rand() % PAGE_NUMBER) != ptAddress); //can not evict the page table of the calling process
	
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
	file = fopen("swapspace.txt", "w+");

	fseek(file, diskAddress, SEEK_SET);

	fwrite(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

	fclose(file);

	pageTaken[evictee] = 1;

	for(int x = evictee * PAGE_SIZE; x < (evictee + 1) * PAGE_SIZE; x++) {
		memory[x] = 0;
	}
	
	//now we must find the entry
	
	for(int x = 0; x < MAX_PROCESS; x++) {
		if(getPTAddress(x) == evictee) {
				hardwareRegister[x] = (diskAddress << 1) | 0;
				return;
		}
	}
	
	//now it wasn't a page table
	//and we need to load the page table of the evictee
	
	
	
	
}

unsigned int ptOnPhysicalMemory(unsigned int process_id) {	
	return (0b01 & hardwareRegister[process_id]) != 0;
}

unsigned int updatePTE(unsigned int process_id, unsigned int vpn, unsigned int pfn, int valid, int mem, int rw) {
	
	if(!ptOnPhysicalMemory(process_id)) {
		//needs to be soft loaded
		
	} else { //can just be edited
		char pageTable = loadPageTable(process_id);
		
		
	}
}


char* loadPT(unsigned int process_id) {
	
	if(!ptOnPhysicalMemory(process_id)) {
		
		char diskAddress = getPTAddress(process_id);
	
		char page[PAGE_SIZE];
		
		FILE *file;
		file = fopen("swapspace.txt", "r+");

		fseek(file, diskAddress, SEEK_SET);

		fread(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

		fclose(file);
		
		return page;
		
	} else {
		char pageTable[PAGE_SIZE];
		
		for(int x = 0; x < PAGE_SIZE; x++) {
			pageTable[x] = memory[getPTAddress(process_id) * PAGE_SIZE + x];
		}
		
		return pageTable;
	}
	
}
void savePT(unsigned int process_id, char *pageTable) {
	if(!ptOnPhysicalMemory(process_id)) {
		FILE *file;
		file = fopen("swapspace.txt", "w+");

		fseek(file, diskAddress, SEEK_SET);

		fwrite(pageTable, sizeof(char), sizeof(char) * PAGE_SIZE, file);

		fclose(file);
	} else {
		
		for(int x = 0; x < PAGE_SIZE; x++) {
			memory[getPTAddress(process_id) * PAGE_SIZE + x] = pageTable[x];
		}
		
	}
	
	printf("The page table for PID %d has been saved\n", process_id);
}



unsigned int  bringFromDisk(unsigned int process_id, unsigned int diskAddress) { //need to update PT
	unsigned int destination = freePage(process_id);
	
	char page[PAGE_SIZE];
	
	FILE *file;
	file = fopen("swapspace.txt", "r+");

	fseek(file, diskAddress, SEEK_SET);

	fread(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

	fclose(file);

	swapSlotTaken[diskAddress] = 1;

	char *putIn = getPageFrame(destination);

	for(int x = 0; x < PAGE_SIZE; x++) {
		putIn[x] = page[x]; //copy things over 
	}
	
	return destination;
}
char getPTAddress(unsigned int process_id) { return (hardwareRegister[process_id] >> 1); }
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
unsigned int onPhysicalMemory(unsigned int process_id, unsigned int vpn) {
	return (getFlagByte(process_id, vpn) & 0b010) != 0;
}
