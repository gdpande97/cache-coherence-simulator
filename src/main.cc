/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include <string.h>
using namespace std;

#include "cache.h"

int COPIES_EXIST;
int protocol;
int c2c_FLAG;
int Flush_no_mem_FLAG; 
int DEBUG_FLAG = 0; // enable debugg printout

int main(int argc, char *argv[])
{
	
	ifstream fin;
	FILE * pFile;
	char *line = NULL;
	size_t len = 0;
	const char *delimiter = " ";
	int proc_id;
	unsigned char op;
	unsigned long addr;
	unsigned int busAction;
	unsigned int checkCount;

	if(argv[1] == NULL){
		 printf("input format: ");
		 printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
		 exit(0);
        }

	/*****uncomment the next five lines*****/
	int cache_size = atoi(argv[1]);
	int cache_assoc= atoi(argv[2]);
	int blk_size   = atoi(argv[3]);
	int num_processors = atoi(argv[4]);/*1, 2, 4, 8*/
	int protocol   = atoi(argv[5]);	 /*0:MSI, 1:MESI, 2:MOSI*/
	char *fname =  (char *)malloc(20);
 	fname = argv[6];

	
	//****************************************************//
	//**printf("===== Simulator configuration =====\n");**//
	//*******print out simulator configuration here*******//
	//****************************************************//

	printf("L1_SIZE: %d\n", cache_size);
	printf("L1_ASSOC: %d\n", cache_assoc);
	printf("L1_BLOCKSIZE: %d\n", blk_size);
	printf("NUMBER OF PROCESSORS: %d\n", num_processors);
	printf("COHERENCE PROTOCOL: %s\n", (protocol == 0)?"MSI":((protocol == 1)?"MESI":((protocol == 2)?"MOSI":"MOESI")));
	printf("TRACE FILE: %.27s\n", &fname[3]); // no "../"

	//*********************************************//
	//*****create an array of caches here**********//
	//*********************************************//	

	Cache *privateCaches[num_processors];
	for(int i = 0; i < num_processors; i++) {
		privateCaches[i] = new Cache(cache_size, blk_size, cache_assoc);
	}

	pFile = fopen (fname,"r");
	if(pFile == 0) {   
		printf("Trace file problem\n");
		exit(0);
	}
	///******************************************************************//
	//**read trace file,line by line,each(processor#,operation,address)**//
	//*****propagate each request down through memory hierarchy**********//
	//*****by calling cachesArray[processor#]->Access(...)***************//
	///******************************************************************//
	while((getline(&line, &len, pFile)) != -1){ //iterate line by line
		// ===== parsing arguments ===============
		proc_id = atoi(strtok(line, delimiter));
		op      = strtok(NULL, delimiter)[0];	
		sscanf(((string)(strtok(NULL, delimiter))).c_str(), "%lx", &addr);

		// cout<<"Processor ID is "<<proc_id << ", Operation is "<<op<<", address is "<<addr<<endl;

		busAction = privateCaches[proc_id]->Access(addr, op, protocol);
		checkCount = 0;
		for(int i = 0; i < num_processors; i++) {
			if(i!=proc_id){
				checkCount += privateCaches[i]->busResponse(busAction, addr);
			}
		}
		privateCaches[proc_id]->sendBusReaction(checkCount, num_processors, addr, protocol, busAction);	
	}
	fclose(pFile);

	//********************************//
	//print out all caches' statistics //
	//********************************//
	for(int i = 0; i < num_processors; i++) {
		privateCaches[i]->printStats(i);
	}

}
