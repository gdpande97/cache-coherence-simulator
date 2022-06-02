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
   writeMisses = writeBacks = currentCycle = 0;

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

   if (op == 'w')
      writes++;
   else
      reads++;

   cacheLine *line = findLine(addr);
   if (line == NULL) /*miss*/
   {
      if (op == 'w')
         writeMisses++;
      else
         readMisses++;

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
            return POLL_MESI; // To handle Exclusive cases
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
      { // MOSI
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
      /**since it's a hit, update LRU and update dirty flag**/
      updateLRU(line);
      if (protocol == 0)
      { // MSI
         if (op == 'w')
         {
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

unsigned int Cache::busResponse(uint protocol, uint busAction, ulong addr)
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
                  writeBack(addr);
                  line->setFlags(VALID);
               }
            }
            else if (busAction == MODIFIED)
            {
               if (line->getFlags() == DIRTY)
               {
                  writeBack(addr);
                  line->setFlags(VALID);
               }
               line->setFlags(INVALID);
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
                  writeBack(addr);
                  line->setFlags(VALID);
               }
               line->setFlags(INVALID);
            }
         }
         else 
         {
            if (busAction == POLL_MESI)
            {
               return 1;
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
                  writeBack(addr);
                  line->setFlags(VALID);
               }
               line->setFlags(INVALID);
            }
            else if (busAction == POLL_MOSI)
            {
               if (line->getFlags() == OWNED || line->getFlags() == DIRTY)
                  line->setFlags(OWNED); // Else leave state as is
            }
         }
      }
   }
   else if (protocol == 3)
   { // MOSI
      if (line != NULL)
      {
         if (line->isValid())
         {
            if (busAction == MODIFIED)
            {
               if (line->getFlags() == DIRTY)
               {
                  writeBack(addr);
                  line->setFlags(VALID);
               }
               line->setFlags(INVALID);
            }
            else if (busAction == POLL_MOESI)
            {
               if (line->getFlags() == OWNED || line->getFlags() == DIRTY){
                  line->setFlags(OWNED); // Else leave state as is
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

void Cache::sendBusReaction(uint count, uint processors, ulong addr, uint protocol, uint busAction)
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
                  line->setFlags(VALID);
               else
                  line->setFlags(EXCLUSIVE);
            }
         }
      }
   }
   else if (busAction == POLL_MOSI)
   {
      cacheLine *line = findLine(addr);
      if (line != NULL)
      {
         if (protocol == 2)
         { // MOSI
            if (line->isValid())
            {
               line->setFlags(VALID);
            }
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
                  line->setFlags(EXCLUSIVE);
               }
            }
         }
      }
   }
}

void Cache::printStats(int proc_id)
{
   printf("===== Simulation results      =====\n");
   /****print out the rest of statistics here.****/
   /****follow the ouput file format**************/
   printf("Processor number : %d\n", proc_id);
   printf("01. number of reads:				%lu\n", reads);
   printf("02. number of read misses:			%lu\n", readMisses);
   printf("03. number of writes:				%lu\n", writes);
   printf("04. number of write misses:			%lu\n", writeMisses);
   printf("05. total miss rate:				%4.2f%%\n", ((double)(readMisses + writeMisses) / (double)(reads + writes)) * 100);
   printf("06. number of writebacks:			%lu\n", writeBacks);
}
