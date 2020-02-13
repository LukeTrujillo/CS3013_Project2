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

#define SWAP_SPACE 64

//two left-most bits are VPN, the rest are the offset

unsigned char memory[PHYSICAL_MEMORY_SIZE];
int hardwareRegister[MAX_PROCESS];

unsigned int pageTaken[PAGE_NUMBER];
unsigned int swapSlotsTake[SWAP_SPACE];

void map(unsigned int, unsigned int, unsigned int);


char const HR_ADDR_MASK = 0b1111110;
char const HR_LOC_MASK = 0b1;


// OLD STUFF below
unsigned int freeList[MAX_PROCESS];

unsigned int swapBlockCounter;

unsigned int const VPN_MASK = 0b110000;
unsigned int const OFFSET_MASK = 0b001111;

char const VALID_BIT_MASK = 0b100;
char const LOC_BIT_MASK = 0b010;
char const WRITE_BIT_MASK = 0b001;

int debug = 1;


int createPageTableBaseRegister(int);
void setup();
char* getNextFreePage(int, int, int);
int getPageTableBaseRegister(int);
void parse(char*, char**);

char* getPageTable(int);

void addPageTableEntry(int, int, int);
int convertVPNtoPFN(int, int);

int getOpenPageNumber(unsigned int);

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

unsigned int getNumberOfPTE(unsigned int);
unsigned int getNumberOfUsedPages(unsigned int);

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


			if(pageExists(process_id, virtual_address & 0b110000) == 1) {

				virtual_address = virtual_address & 0b110000;

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

	for(int x = 0; x < PAGE_NUMBER; x++) {
		pageTaken[x] = 1;
	}

	for(int x = 0; x < PAGE_NUMBER; x++) {
		swapSlotTaken[x] = 1;
	}

}

int getPageTableBaseRegister(int process_id) {


	//printf("getPageTableTable = %d \n", hardwareRegister[0]);
	if(hardwareRegister[process_id] != -1) {
		return hardwareRegister[process_id];
	} 

	return -1;
}
int createPageTableBaseRegister(int process_id) {
	for(int x = 0; x < MAX_PROCESS; x++) {
		if(freeList[x] == 1 && hardwareRegister[process_id] == -1) {
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

void map(unsigned int process_id, unsigned int vpn, unsigned int rwFlag) {
	int shouldCreate = !hasPageTable(process_id);

	if(shouldCreate) {
		createPageTable(process_id); //create the page table
	}

	addPTE(process_id, vpn, rwFlag); //otherwise just add and allocate the space
	
}

void createPageTable(unsigned int process_id) {
	unsigned int makeLocation = freePage(); //will either swap the page or return a free one	
}

/*
	
*/
unsigned int freePage() {
	//check if there is already a free page, if there is return the pfn
	for(int x = 0; x < PAGE_NUMBER; x++) {
		if(pageTaken[x] == 1) { return x; }
	}

	//choose who to swap
	srand(time(0));

	int evictee = rand() % PAGE_NUMBER; //the page number to be evicted
	
	//swap out
	char diskAddress = bringToDisk(evictee);

	updatePTEOnDisk(process_id, evictee, diskAddress); //find the PTE, add the disk address, and change the flag

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
			if(pageTable[y] == evictee && pageTable[y + 3] & LOC_BIT_MASK) {
				
				printf("PFN %d swapped into swap space %d\n", evictee, diskAddress);

				pageTable[y + 1] = diskAddress; 
				pageTable[y + 3] = pageTable[y + 3] & ~(LOC_BIT_MASK);

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
	
	//it should now be on the disk

	return diskAddress;
}




unsigned int getPFN(unsigned int process_id, unsigned int vpn) {
	char *pageTable = loadPageTable(process_id); //this will return the pageTable, or swap it in and then return the offset

}
char* loadPageTable(unsigned int process_id) {
	if(!hardwareRegister[process_id] & HR_LOC_MASK) { //if the page table is not loaded 
		//load it 

		

	} 

	//now its on the disk

	char *pageTable;

	pageTable = &memory[(hardwareRegister[process_id] & (HR_ADDR_MASK >> 1)) * PAGE_SIZE];

	return pageTable;
}

unsigned int hasPageTable(unsigned int process_id) { return hardwareRegister != -1; }




char* getNextFreePage(int process_id, int vpn, int rwFlag) {
	int startingIndex = createPageTableBaseRegister(process_id);

	if(getNumberOfUsedPages(process_id) == 3){
		printf("we need to swap\n");
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

	int open = getOpenPageNumber(process_id);

	if(open != -1) {


		vpn = vpn & 0b110000;
	
		printf("Mapped virtual address %d (page %d) into physical frame %d\n", vpn, open, ((startingIndex + (open) * PAGE_SIZE) / PAGE_SIZE) + 1);

		memory[startingIndex + vpn * PAGE_TABLE_ENTRY_SIZE] = (open);

		//last byte: 0000 0 loc,valid,rw
		setFlags(process_id, vpn, 1, 1, rwFlag);
		
	}
		
}
int getOpenPageNumber(unsigned int process_id) {
	char *pageTable = getPageTable(process_id);

	//determine the

	int pages[3] = {0, 0, 0};

	for(int x = 0; x < PAGE_SIZE; x += PAGE_TABLE_ENTRY_SIZE) {

		int vpn = (x / PAGE_TABLE_ENTRY_SIZE);


		if(isValid(process_id, vpn)  && onPhysicalMemory(process_id, vpn)) {

			
			if(pageTable[x] == 0) {
				pages[0] = 1;
			} else if(pageTable[x] == 1) {
				pages[1] = 1;
			} else if(pageTable[x] == 2) {
				pages[2] = 1;
			}

			continue;
		}
	}


	for(int x = 0; x < 3; x++) {
		if(pages[x] == 0) {
			return x;

		}
	}

	return -1;
		
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
	printf("VPN: %d PFN: %d d_addr: %d valid: %d in_mem: %d rw: %d\n", vpn, getPFN(process_id, vpn), getDiskAddress(process_id, vpn),isValid(process_id, vpn), onPhysicalMemory(process_id, vpn), isWritable(process_id, vpn));

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

/*
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
	
		

	int val = 1;

	for(int x = 1; x < PAGE_NUMBER; x++) {
	
		if(processFreeList[process_id][x] == 1) {

			val = 0;
		}
	}
	return val;
}


unsigned int getNumberOfPTE(unsigned int process_id) {
	int entry_count = 0;

	for(int x = 0; x < PAGE_SIZE; x += PAGE_TABLE_ENTRY_SIZE) {
		char flag_byte = memory[getPageTableBaseRegister(process_id) + x + 3];

		if(flag_byte & VALID_BIT_MASK != 0) {
			entry_count++;
		}
	} 

	return entry_count;
}

unsigned int getNumberOfUsedPages(unsigned int process_id) {
	int used_count = 0;

	for(int x = 0; x < PAGE_SIZE / PAGE_TABLE_ENTRY_SIZE; x++) {

		if(isValid(process_id, x) && onPhysicalMemory(process_id, x)) {
			used_count++;
		}
	} 

	return used_count;

}


