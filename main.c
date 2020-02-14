/*
 *	main.c
 *	Authors: Luke Trujillo, Mike Capobianco
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

unsigned int ptOnPhysicalMemory(unsigned int);


char freePage(unsigned int);
void updatePTEOnDisk(unsigned int, unsigned int);
char bringToDisk (unsigned int);
char getPTAddress(unsigned int);

char* getPageFrame(unsigned int);
char* loadPageTable(unsigned int);
unsigned int hasPageTable(unsigned int);

unsigned int vpnExists(unsigned int, unsigned int);
unsigned int isValid (unsigned int, unsigned int);
unsigned int onPhysicalMemory(unsigned int, unsigned int);

void loadPage(unsigned int, unsigned int, char*);
void savePage(unsigned int, unsigned int, char*);

void printPT();

char const HR_ADDR_MASK = 0b1111110;
char const HR_LOC_MASK = 0b1;

void evict(unsigned int);
char* loadPT(unsigned int, char*);
void savePT(unsigned int, char*);
void printPage(unsigned int);

void printSwapSpace();

int main(int argc, char** argv) {
	setup();
	while(1) {
		char word[100];
		char* args[4];
		printf("\nInstruction? ");
		scanf("%s", word); 

		printf("%s\n", word);
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
			//printPT();
			continue;
		}
		
		
		//make sure the vpn is loaded 
		int vpn = (virtual_address & 0b110000) >> 4;
		
		 if(strcmp(command, "store") == 0) {
			
			
			if(isWritable(process_id, vpn)) {
				
				char page[PAGE_SIZE];
				loadPage(process_id, vpn, page);
				
				unsigned int pfn = getPFN(process_id, vpn);
				

				unsigned int offset = virtual_address & 0b1111;
				unsigned int physical_address = (PAGE_SIZE * pfn) + offset;

				memory[physical_address] = value;
				
				
				printf("Stored value %d at virtual address %d (physical address %d)\n", value, virtual_address, physical_address);
				
				//printPage(pfn);
			} else {
				printf("Error: writes are not allowed to this page\n");
			}
		} else if(strcmp(command, "load") == 0) {
				//is on disk
				char page[PAGE_SIZE];
				loadPage(process_id, vpn, page);
				
				unsigned int pfn = getPFN(process_id, vpn);
				
				//printPage(pfn);
				
				unsigned int offset = virtual_address & 0b1111;
				unsigned int physical_address = (PAGE_SIZE * pfn) + offset;
			
				unsigned char val = getPageFrame(pfn)[offset];
			
				printf("The value %d is virtual address %d (physical address %d)\n", val, virtual_address, physical_address);
		} 
		//printPT();
	}

	return 0;
}

void printSwapSpace() {
	
	char data[PHYSICAL_MEMORY_SIZE];
	
	FILE *file;
	file = fopen("swapspace.txt", "r");

	fseek(file, 0, SEEK_SET);

	fread(data, sizeof(char), sizeof(char) * PHYSICAL_MEMORY_SIZE, file);

	fclose(file);
	
	
	printf("Swap Space: ");
	for(int x = 0; x < PHYSICAL_MEMORY_SIZE; x++) {
	
		if(x % PAGE_SIZE == 0) {
				printf("\n %d: ", x / PAGE_SIZE);
		}
		printf("%d ", data[x]);
	}
	printf("\n");
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
	
	
	
	FILE *file;
	file = fopen("swapspace.txt", "w+");


	fseek(file, 0, SEEK_SET);

	fwrite(memory, sizeof(char), sizeof(char) * PHYSICAL_MEMORY_SIZE, file);
	fclose(file);
	
	//sprintSwapSpace();

}
void map(char process_id, char vpn, char rwFlag) {
	int shouldCreate = !hasPageTable(process_id);

	if(shouldCreate) {
		createPageTable(process_id); //create the page table
	}

	addPTE(process_id, vpn, rwFlag); //otherwise just add and allocate the space
	
}

void printPage(unsigned int pfn) {
	printf("PFN(%d): ", pfn);
	
	for(int x = 0; x < PAGE_SIZE; x++) {
			printf("%d ", memory[pfn  * PAGE_SIZE + x]);
	}
	printf("\n");
}

void printPT() {
	
	printf("\n\n---SUMMARY---\n");
	for(int i = 0; i < MAX_PROCESS; i++) {
		if(hardwareRegister[i] == -1) continue;
		
		printf("PID #%d addr: %d: ", i, getPTAddress(i));
		
		char pageTable[16]; 
		loadPT(i, pageTable);
		
		for(int x = 0; x < PAGE_SIZE; x++) {
				printf("%d ", pageTable[x]);
		}
		
		printf("\n");
	}
}
void addPTE(char process_id, char vpn, char rwFlag) {
	
	setFlagByte(process_id, vpn, 1, 1, rwFlag);
	
	char page = freePage(process_id);
	
	char pageTable[16]; 
	loadPT(process_id, pageTable);

	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE] = page;
	
	printf("Mapped virtual address %d into physical frame %d\n", vpn, page);
	
	savePT(process_id, pageTable);
}
void createPageTable(unsigned int process_id) {

	//printf("PID %d has no page table, making one...\n", process_id); 
	unsigned int makeLocation = freePage(process_id); //will either swap the page or return a free one
	

	setPTAddress(process_id, makeLocation, 1);
	
	pageTaken[makeLocation] = 0;
	
	printf("Put page table for PID %d into physical frame %d\n", process_id, makeLocation); //may be wrong place 

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
	evict(process_id);
	
	return freePage(process_id);
	
}
char* getPageFrame(unsigned int pfn) {
	return &memory[pfn * PAGE_SIZE];
}
void evict(unsigned int process_id) {
	
	char ptAddress = getPTAddress(process_id); //can not evict this
	
	srand(time(0));
	char evictee = rand() % PAGE_NUMBER; //the page number to be evicted
	
	while((evictee = rand() % PAGE_NUMBER) == ptAddress); //can not evict the page table of the calling process
	
	
	//find the PT that the evictee corresponds to 
	
	
	unsigned int evicteePID = -1;
	unsigned int evicteeVPN = -1;
	
	unsigned int isPT = 0;
	
	for(int x = 0; x < MAX_PROCESS; x++) {
		
			if(hardwareRegister[x] == -1) continue;
			
			char pageTable[PAGE_SIZE];
			loadPT(x, pageTable);
			
			
			
			if(getPTAddress(x) == evictee && !ptOnPhysicalMemory(x)) { //we are victing a page table
				evicteePID = x;
				break;
				
			}
			
		
			for(int y = 0; y < PAGE_SIZE; y++) {
				
				
				if(pageTable[y] == evictee && onPhysicalMemory(x, (y / PAGE_TABLE_ENTRY_SIZE))) {
					evicteePID = x;
					evicteeVPN = (y / PAGE_TABLE_ENTRY_SIZE);
					
					break;
					
				}	
			}
	}

	
	/*if((evicteePID == -1 || evicteeVPN == -1) && !isPT) {
			printf("could not find PID/VPN %d/%d for evictee %d\n", evicteePID, evicteeVPN, evictee);
			return;
	}*/
	
	char evicteePageTable[PAGE_SIZE];
	loadPT(evicteePID, evicteePageTable);
	pageTaken[evictee] = 1;
	
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
	file = fopen("swapspace.txt", "r+");

	fseek(file, diskAddress * PAGE_SIZE, SEEK_SET);

	fwrite(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);

	fclose(file);

	for(int x = 0; x < PAGE_SIZE; x++) {
		memory[evictee * PAGE_SIZE + x] = 0;
	}
	
	//now we must find the entry
	printf("Swapped frame %d into swap spot %d\n", evictee, diskAddress);
	
	if(getPTAddress(evicteePID) == evictee) {
			hardwareRegister[evicteePID] = (diskAddress << 1) | 0;
			return;
	}
	
	evicteePageTable[evicteeVPN * PAGE_TABLE_ENTRY_SIZE + 1] = diskAddress;
	
	//update the page location flag
	evicteePageTable[evicteeVPN * PAGE_TABLE_ENTRY_SIZE + 3] = evicteePageTable[evicteeVPN * PAGE_TABLE_ENTRY_SIZE + 3] & (~0b010);
	
	savePT(evicteePID, evicteePageTable);

	
	//printSwapSpace();

}


void loadPage(unsigned int process_id, unsigned int vpn, char *page) {
	
	if(!onPhysicalMemory(process_id, vpn)) {
		//needs to be loaded 
		
		unsigned int emptySpace = freePage(process_id);
		
		char pageTable[PAGE_SIZE];
		loadPT(process_id, pageTable);
		
		char diskAddress = pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 1];
		
		FILE *file;
		
		file = fopen("swapspace.txt", "r");
		fseek(file, diskAddress * PAGE_SIZE, SEEK_SET);
		fread(page, sizeof(char), sizeof(char) * PAGE_SIZE, file);
		
		fclose(file);
			
		for(int x = 0; x < PAGE_SIZE; x++) {
				memory[emptySpace * PAGE_SIZE + x] = page[x];
		}
		
		pageTable[vpn * PAGE_TABLE_ENTRY_SIZE] = emptySpace;
		pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 3] = pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 3] | 0b010;
		
		savePT(process_id, pageTable);
		
		printf("Swapped disk slot %d into frame %d\n", diskAddress, emptySpace);
	}
	
	//now it should be loaded
	
	if(onPhysicalMemory(process_id, vpn)) {
		unsigned int pfn = getPFN(process_id, vpn);
		page = getPageFrame(process_id);
	} else {
		printf("Error with system R/W permissions.\n");
	}
	
	
}

void savePage(unsigned int process_id, unsigned int vpn, char *page) {
		
		if(onPhysicalMemory(process_id, vpn)) {
			unsigned int pfn = getPFN(process_id, vpn);
			
			
			for(int x = 0; x < PAGE_SIZE; x++) {
				memory[pfn * PAGE_SIZE + x] = page[x];
			}

		}
}

unsigned int ptOnPhysicalMemory(unsigned int process_id) {	
	return (0b01 & hardwareRegister[process_id]) != 0;
}

unsigned int updatePTE(unsigned int process_id, unsigned int vpn, unsigned int pfn, char diskAddress, int valid, int mem, int rw) {
	
	char pageTable[16];
	 loadPT(process_id, pageTable);
	
	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE] = pfn;
	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 1] = diskAddress;
	
	
	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 3] =  (mem << 1) | (rw << 0);
	
	savePT(process_id, pageTable);
}

char* loadPT(unsigned int process_id, char* pageTable) {
	if(hardwareRegister[process_id] == -1) { return NULL; }
	
	if(!ptOnPhysicalMemory(process_id)) {
		
		char diskAddress = getPTAddress(process_id);
		
		FILE *file;
		file = fopen("swapspace.txt", "r");

		fseek(file, diskAddress * PAGE_SIZE, SEEK_SET);

		fread(pageTable, sizeof(char), sizeof(char) * PAGE_SIZE, file);

		fclose(file);
		
	} else {
		
		for(int x = 0; x < PAGE_SIZE; x++) {
			pageTable[x] = memory[getPTAddress(process_id) * PAGE_SIZE + x];
		}
		
	}
	
}
void savePT(unsigned int process_id, char *pageTable) {
	if(hardwareRegister[process_id] == -1) { return; }
	
	if(!ptOnPhysicalMemory(process_id)) {
		FILE *file;
		file = fopen("swapspace.txt", "r+");
		
		
		char diskAddress = getPTAddress(process_id);

		fseek(file, diskAddress * PAGE_SIZE, SEEK_SET);

		fwrite(pageTable, sizeof(char), sizeof(char) * PAGE_SIZE, file);

		fclose(file);
	} else {
		
		for(int x = 0; x < PAGE_SIZE; x++) {
			memory[getPTAddress(process_id) * PAGE_SIZE + x] = pageTable[x];
		}
		
	}
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
	char pageTable[16];
	loadPT(process_id, pageTable);
	
	return pageTable[vpn * PAGE_TABLE_ENTRY_SIZE];
}
char getFlagByte(unsigned int process_id, unsigned int vpn) {
	char pageTable[16];
	loadPT(process_id, pageTable);
	
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
	
	char pageTable[16];
	loadPT(process_id, pageTable);
	pageTable[vpn * PAGE_TABLE_ENTRY_SIZE + 3] = flagByte;
	
	savePT(process_id, pageTable);
}
unsigned int isWritable(unsigned int process_id, unsigned int vpn) {
	char flagByte = getFlagByte(process_id, vpn);
	
	//printf("isWritable flagByte: %d\n", (flagByte & 1));
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
