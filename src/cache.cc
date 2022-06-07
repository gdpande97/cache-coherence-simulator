/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s, int a, int b)
{
   ulong i, j;
   reads = readMisses = writes = 0;
   writeMisses = writeBacks = currentCycle = getMMsgs = getSMsgs = 0;
   invalidations = currentHit = inc = sendDatatoMem = silentUpgrade = servicedFromMem = servicedFromOtherCore = 0;
   readHits = writeHits = 0;
   size = (ulong)(s);
   lineSize = (ulong)(b);
   assoc = (ulong)(a);
   sets = (ulong)((s / b) / a);
   numLines = (ulong)(s / b);
   log2Sets = (ulong)(log2(sets));
   log2Blk = (ulong)(log2(b));

   //*******************//
   // initialize your counters here//
   //*******************//

   tagMask = 0;
   for (i = 0; i < log2Sets; i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }

   /**create a two dimentional cache, sized as cache[sets][assoc]**/
   cache = new cacheLine *[sets];
   for (i = 0; i < sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for (j = 0; j < assoc; j++)
      {
         cache[i][j].invalidate();
      }
   }
}

/**you might add other parameters to Access()
since this function is an entry point
to the memory hierarchy (i.e. caches)**/
unsigned int Cache::Access(ulong addr, uchar op, uint protocol)
{
   currentCycle++; /*per cache global counter to maintain LRU order
          among cache ways, updated on every cache access*/
   currentHit = 0;
   inc = 0;

   if (op == 'w')
   {
      writes++;
   }
   else
   {
      reads++;
   }

   cacheLine *line = findLine(addr);
   if (line == NULL) /*miss*/
   {
      if (op == 'w')
      {
         writeMisses++;
         getMMsgs++; // Write miss can never have state silent change to M state for any protocol
      }
      else
      {
         getSMsgs++; // Read miss can never have state silent change to M state for any protocol
         readMisses++;
      }

      cacheLine *newline = fillLine(addr);
      if (protocol == 0)
      { // MSI
         if (op == 'w')
         {
            newline->setFlags(DIRTY);
            return MODIFIED;
         }
         else
            return SHARED;
      }
      else if (protocol == 1)
      { // MESI
         if (op == 'w')
         {
            newline->setFlags(DIRTY);
            return MODIFIED;
         }
         else
         {
            return POLL_MESI; // To handle Exclusive cases
         }
      }
      else if (protocol == 2)
      { // MOSI
         if (op == 'w')
         {
            newline->setFlags(DIRTY);
            return MODIFIED;
         }
         else
            return POLL_MOSI; // ALways go in shared state for MOSI on read Miss
      }
      else if (protocol == 3)
      { // MOESI
         if (op == 'w')
         {
            newline->setFlags(DIRTY);
            return MODIFIED;
         }
         else
            return POLL_MOESI; //
      }
      else
      {
         cout << "Undefined protocol - Should Not reach here, returning no action" << endl;
         return NOACTION;
      }
   }
   else
   {
      if (op == 'w')
      {
         writeHits++;
      }
      else
      {
         readHits++;
      }
      currentHit = 1;
      /**since it's a hit, update LRU and update dirty flag**/
      updateLRU(line);
      if (protocol == 0)
      { // MSI
         if (op == 'w')
         {
            if (line->getFlags() == VALID)
            {
               getMMsgs++; // Ownership message sent if in S state for MSI, can't be in I state here
            }
            line->setFlags(DIRTY);
            return MODIFIED;
         }
         else
         {
            return NOACTION;
         }
      }
      else if (protocol == 1)
      { // MESI
         if (op == 'w')
         {
            if (line->getFlags() == VALID)
            {
               getMMsgs++; // Ownership message sent if in S state for MESI, can't be in I state here.. E->M is silent
            }
            else if (line->getFlags() == EXCLUSIVE)
            {
               silentUpgrade++;
            }
            line->setFlags(DIRTY);
            return MODIFIED;
         }
         else
         {
            return NOACTION;
         }
      }
      else if (protocol == 2)
      { // MOSI
         if (op == 'w')
         {
            if (line->getFlags() == VALID || line->getFlags() == OWNED)
            {
               getMMsgs++; // Ownership message sent if in S/O state for MOSI, can't be in I state here..
            }
            line->setFlags(DIRTY);
            return MODIFIED;
         }
         else
         {
            return NOACTION;
         }
      }
      else if (protocol == 3)
      { // MOSI
         if (op == 'w')
         {
            if (line->getFlags() == VALID || line->getFlags() == OWNED)
            {
               getMMsgs++; // Ownership message sent if in S/O state for MOESI, can't be in I state here.. E->M is silent
            }
            else if (line->getFlags() == EXCLUSIVE)
            {
               silentUpgrade++;
            }
            line->setFlags(DIRTY);
            return MODIFIED;
         }
         else
         {
            return NOACTION;
         }
      }
      else
      {
         cout << "Undefined protocol - Should Not reach here, returning no action" << endl;
         return NOACTION;
      }
   }
}

/*look up line*/
cacheLine *Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;

   pos = assoc;
   tag = calcTag(addr);
   i = calcIndex(addr);

   for (j = 0; j < assoc; j++)
      if (cache[i][j].isValid())
         if (cache[i][j].getTag() == tag)
         {
            pos = j;
            break;
         }
   if (pos == assoc)
      return NULL;
   else
      return &(cache[i][pos]);
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine *Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min = currentCycle;
   i = calcIndex(addr);

   for (j = 0; j < assoc; j++)
   {
      if (cache[i][j].isValid() == 0)
         return &(cache[i][j]);
   }
   for (j = 0; j < assoc; j++)
   {
      if (cache[i][j].getSeq() <= min)
      {
         victim = j;
         min = cache[i][j].getSeq();
      }
   }
   assert(victim != assoc);

   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine *victim = getLRU(addr);
   updateLRU(victim);

   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{
   ulong tag;

   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);
   if (victim->getFlags() == DIRTY)
      writeBack(addr);

   tag = calcTag(addr);
   victim->setTag(tag);
   victim->setFlags(VALID);
   /**note that this cache line has been already
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

unsigned int Cache::busResponse(uint protocol, uint busAction, ulong addr, uint &incServicedFromOtherCore, uint &incServicedFromMem)
{
   cacheLine *line = findLine(addr);
   if (protocol == 0)
   { // MSI
      if (line != NULL)
      {
         if (line->isValid())
         {
            if (busAction == SHARED)
            {
               if (line->getFlags() == DIRTY)
               {
                  incServicedFromOtherCore++; // Sends data to memory other core on otherGetS in dirty state
                  writeBack(addr);            // Sends data to memory on otherGetS in dirty state
                  line->setFlags(VALID);
               }
               else
               {
                  incServicedFromMem++; // in MSI only Dirty state can provide data to requester else it is memory
               }
            }
            else if (busAction == MODIFIED)
            {
               if (line->getFlags() == DIRTY)
               {
                  incServicedFromOtherCore++; // Sends data to other core on otherGetM in dirty state
                  invalidations++;            // Updates whenever M -> I
                  line->setFlags(INVALID);
                  // writeBack(addr); //No need to send data to memory if its a OtherGETM while you are in Dirty state
               }
               else if (line->getFlags() == VALID)
               {
                  invalidations++; // Updates whenever S -> I
                  line->setFlags(INVALID);
               }
            }
         }
      }
   }
   else if (protocol == 1)
   { // MESI
      if (line != NULL)
      {
         if (line->isValid())
         {
            if (busAction == MODIFIED)
            {
               if (line->getFlags() == DIRTY)
               {
                  incServicedFromOtherCore++; // Send data to requester if in M state
                  // writeBack(addr); //No need to send data to memory if its a OtherGETM while you are in Dirty state
                  invalidations++; // Updates whenever M -> I
                  line->setFlags(INVALID);
               }
               else if (line->getFlags() == EXCLUSIVE)
               {
                  incServicedFromOtherCore++; // Send data to requester if in E state
                  invalidations++;            // Updates whenever E-> I
                  line->setFlags(INVALID);
               }
               else
               {
                  if (!currentHit & (inc == 0))
                  {
                     inc = 1;              // Set here on firt time so that we don't double count (something like 10 sharers and 1 Modified comes)
                     incServicedFromMem++; // Serviced from memory if it was a miss
                  }
                  invalidations++; // Updates whenever S -> I
                  line->setFlags(INVALID);
               }
            }
            else if (busAction == POLL_MESI)
            {
               if (line->getFlags() == DIRTY)
               {
                  incServicedFromOtherCore++; // Send data to requester if in M state
                  writeBack(addr);            // need to send data to memory if its a OtherGETS while you are in Dirty state
                  invalidations++;            // Updates whenever M -> I
                  line->setFlags(INVALID);
               }
               else if (line->getFlags() == EXCLUSIVE)
               {
                  incServicedFromOtherCore++; // Send data to requester if in E state
                  sendDatatoMem++;
                  line->setFlags(VALID);
               }
            }
         }
         else
         {
            if (busAction == POLL_MESI)
            {
               return 1; // Used to check if the requesting block will go to Exclusive state or not
            }
         }
      }
   }
   else if (protocol == 2)
   { // MOSI
      if (line != NULL)
      {
         if (line->isValid())
         {
            if (busAction == MODIFIED)
            {
               if (line->getFlags() == DIRTY)
               {
                  incServicedFromOtherCore++; // Sends data to other core on otherGetM state
                  // writeBack(addr); //Data not sent to memory if otherGetM done in DIrty state
                  invalidations++; // Updates whenever M -> I
                  line->setFlags(INVALID);
               }
               else if (line->getFlags() == OWNED)
               {
                  incServicedFromOtherCore++; // Sends data to other core on otherGetM state
                  invalidations++;            // Updates whenever O-> I
                  line->setFlags(INVALID);
               }
               else if (line->getFlags() == SHARED)
               {
                  invalidations++; // Updates whenever S -> I
                  line->setFlags(INVALID);
               }
            }
            else if (busAction == POLL_MOSI)
            {
               if (line->getFlags() == OWNED || line->getFlags() == DIRTY)
               {
                  incServicedFromOtherCore++;
                  line->setFlags(OWNED); // Else leave state as is
               }
               else if (line->getFlags() == VALID)
               {
                  return 1;
               }
            }
         }
         else if (busAction == POLL_MOSI)
         {
            if (line->getFlags() == INVALID)
            {
               return 1;
            }
         }
      }
   }
   else if (protocol == 3)
   { // MOESI
      if (line != NULL)
      {
         if (line->isValid())
         {
            if (busAction == MODIFIED)
            {
               if (line->getFlags() == DIRTY || line->getFlags() == OWNED || line->getFlags() == EXCLUSIVE)
               {
                  incServicedFromOtherCore++; // Sends data to other core on otherGetM state
                  // writeBack(addr); //Data not sent to memory if otherGetM done in M/O/E state
                  invalidations++; // Updates whenever M/O/E -> I
                  line->setFlags(INVALID);
               }
               else if (line->getFlags() == SHARED)
               {
                  // servicedFromMem++; //Cant add here since a block in O state can also send data... //FIXME
                  invalidations++; // Updates whenever S -> I
                  line->setFlags(INVALID);
               }
            }
            else if (busAction == POLL_MOESI)
            {
               if (line->getFlags() == OWNED || line->getFlags() == DIRTY)
               {
                  incServicedFromOtherCore++; // Sends data to other core on otherGetM state
                  line->setFlags(OWNED);      // Else leave state as is
               }
               else if (line->getFlags() == EXCLUSIVE)
               {
                  incServicedFromOtherCore++; // Sends data to other core on otherGetM state
                  line->setFlags(VALID);
               }
               else if (line->getFlags() == VALID)
               {
                  return 1;
               }
            }
         }
         else
         {
            if (busAction == POLL_MOESI)
            {
               if (line->getFlags() == INVALID)
               {
                  return 1;
               }
            }
         }
      }
   }
   return 0;
}

void Cache::sendBusReaction(uint count, uint processors, ulong addr, uint protocol, uint busAction, uint &incServicedFromOtherCore, uint &incServicedFromMem)
{
   if (busAction == POLL_MESI)
   {
      cacheLine *line = findLine(addr);
      if (line != NULL)
      {
         if (protocol == 1)
         { // MESI
            if (line->isValid())
            {
               if (count != processors - 1)
               {
                  line->setFlags(VALID);
               }
               else
               {
                  line->setFlags(EXCLUSIVE);
                  incServicedFromMem++; // If it has reached here, it means all other blocks are in invalid state and memory sent this data
               }
            }
         }
      }
   }
   else if (busAction == POLL_MOSI)
   {
      // servicedFromOtherCore++; // Redundant to keep here, nothign meaningful happening here in this state
      cacheLine *line = findLine(addr);
      if (line != NULL)
      {
         if (protocol == 2)
         { // MOSI
            if (count == processors - 1)
            {
               incServicedFromMem++; // If it has reached here, it means all other blocks are in invalid/Shared state and memory sent this data
            }
            line->setFlags(VALID);
         }
      }
   }
   else if (busAction == POLL_MOESI)
   {
      cacheLine *line = findLine(addr);
      if (line != NULL)
      {
         if (protocol == 3)
         { // MOESI
            if (line->isValid())
            {
               if (count != processors - 1)
               {
                  line->setFlags(VALID);
               }
               else
               {
                  incServicedFromMem++; // If it is set to exclusive then it is serviced from memory
                  line->setFlags(EXCLUSIVE);
               }
            }
         }
      }
   }
}

void Cache::printState(ulong addr, int cache_num)
{
   string state;
   cacheLine *line = findLine(addr);

   if (line != NULL)
   {
      switch (line->getFlags())
      {
      case INVALID:
         state = "I";
         break;
      case VALID:
         state = "S";
         break;
      case OWNED:
         state = "O";
         break;
      case DIRTY:
         state = "M";
         break;
      case EXCLUSIVE:
         state = "E";
         break;
      }
      cout << "In cache " << cache_num << " Address: " << addr << " State: " << state << endl;
   }
   else
   {
      cout << "In cache " << cache_num << " Address: " << addr << " State: I" << endl;
   }
}

void Cache::updateStats(uint incServicedFromOtherCore, uint incServicedFromMem)
{
   servicedFromMem += incServicedFromMem;
   servicedFromOtherCore += incServicedFromOtherCore;
}

void Cache::printStats(int proc_id)
{
   printf("===== Simulation results      =====\n");
   /****print out the rest of statistics here.****/
   /****follow the ouput file format**************/
   printf("Processor number : %d\n", proc_id);
   printf("01. number of reads:				%lu\n", reads);
   printf("02a. number of read misses:			%lu\n", readMisses);
   printf("02b. number of read Hits:        %lu\n", readHits);
   printf("03. number of writes:				%lu\n", writes);
   printf("04a. number of write misses:			%lu\n", writeMisses);
   printf("04b. number of write Hits:       %lu\n", writeHits);
   printf("05. total miss rate:				%4.2f%%\n", ((double)(readMisses + writeMisses) / (double)(reads + writes)) * 100);
   printf("06. number of writebacks:			%lu\n", writeBacks);
   printf("07. number of invalidations:         %lu\n", invalidations);
   printf("08. number of getMMsgs:         %lu\n", getMMsgs);
   printf("09. number of servicedFromMem:         %lu\n", servicedFromMem);
   printf("10. number of servicedFromOtherCore:         %lu\n", servicedFromOtherCore);
   printf("10. number of sendDatatoMem:         %lu\n", sendDatatoMem);
   printf("12. number of getSMsgs:         %lu\n", getSMsgs);
}
