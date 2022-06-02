/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/
enum
{
   INVALID = 0,
   VALID, // Equivalent to shared in MSI, MOSI, MESI
   EXCLUSIVE,
   OWNED,
   DIRTY
};
enum
{
   MODIFIED = 0,
   SHARED = 1,
   NOACTION = 2,
   POLLOTHERS = 3,
   POLL_MOSI = 4
} possibleBusAction;

class cacheLine
{
protected:
   ulong tag;
   ulong Flags; // 0:invalid, 1:valid, 2:dirty
   ulong seq;

public:
   cacheLine()
   {
      tag = 0;
      Flags = 0;
   }
   ulong getTag() { return tag; }
   ulong getFlags() { return Flags; }
   ulong getSeq() { return seq; }
   void setSeq(ulong Seq) { seq = Seq; }
   void setFlags(ulong flags) { Flags = flags; }
   void setTag(ulong a) { tag = a; }
   void invalidate()
   {
      tag = 0;
      Flags = INVALID;
   } // useful function
   bool isValid() { return ((Flags) != INVALID); }
};

class Cache
{
protected:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
   ulong reads, readMisses, writes, writeMisses, writeBacks;

   //******///
   // add coherence counters here///
   //******///

   cacheLine **cache;
   ulong calcTag(ulong addr) { return (addr >> (log2Blk)); }
   ulong calcIndex(ulong addr) { return ((addr >> log2Blk) & tagMask); }
   ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk)); }

public:
   ulong currentCycle;

   Cache(int, int, int);
   ~Cache() { delete cache; }

   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine *findLine(ulong addr);
   cacheLine *getLRU(ulong);

   ulong getRM() { return readMisses; }
   ulong getWM() { return writeMisses; }
   ulong getReads() { return reads; }
   ulong getWrites() { return writes; }
   ulong getWB() { return writeBacks; }

   void writeBack(ulong) { writeBacks++; }
   unsigned int Access(ulong, uchar, uint);
   void printStats(int);
   void updateLRU(cacheLine *);
   unsigned int busResponse(uint, uint, ulong);
   void sendBusReaction(uint, uint, ulong, uint, uint);
   //******///
   // add other functions to handle bus transactions///
   //******///
};

#endif
