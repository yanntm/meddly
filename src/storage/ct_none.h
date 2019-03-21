
/*
    Meddly: Multi-terminal and Edge-valued Decision Diagram LibrarY.
    Copyright (C) 2009, Iowa State University Research Foundation, Inc.

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published 
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef CT_NONE_H
#define CT_NONE_H

// **********************************************************************
// *                                                                    *
// *                                                                    *
// *                           ct_none  class                           *
// *                                                                    *
// *                                                                    *
// **********************************************************************

namespace MEDDLY {
  template <bool MONOLITHIC, bool CHAINED>
  class ct_none : public compute_table {
    public:
      ct_none(const ct_initializer::settings &s, operation* op, unsigned slot);
      virtual ~ct_none();

      /**
          Find an entry.
          Used by find() and updateEntry().
            @param  key   Key to search for.
            @return Pointer to the result portion of the entry, or null if not found.
      */
      entry_item* findEntry(entry_key* key);

      // required functions

      virtual bool isOperationTable() const   { return !MONOLITHIC; }
      virtual void find(entry_key* key, entry_result &res);
      virtual void addEntry(entry_key* key, const entry_result& res);
      virtual void updateEntry(entry_key* key, const entry_result& res);
      virtual void removeStales();
      virtual void removeAll();
      virtual void show(output &s, int verbLevel = 0);

    private:  // helper methods

      size_t convertToList(bool removeStales);
      void listToTable(size_t h);

      void scanForStales();
      void rehashTable(const size_t* oldT, size_t oldS);

      /// Grab space for a new entry
      node_address newEntry(unsigned size);

      // TBD - migrate to 64-bit hash

      static unsigned hash(const entry_key* key);
      static unsigned hash(const entry_type* et, const entry_item* entry);

      inline void incMod(size_t &h) {
        h++;
        if (h>=tableSize) h=0;
      }

      /// Update stats: we just searched through c items
      inline void sawSearch(unsigned c) {
        if (c>=stats::searchHistogramSize) {
          perf.numLargeSearches++;
        } else {
          perf.searchHistogram[c]++;
        }
        if (c>perf.maxSearchLength) perf.maxSearchLength = c;
      }

      /**
          Try to set table[h] to curr.
          If the slot is full, check ahead the next couple for empty.
          If none are empty, then recycle current table[h]
          and overwrite it.
      */
      inline void setTable(size_t h, size_t curr) {
        MEDDLY_DCASSERT(!CHAINED);
        size_t hfree = h;
        for (int i=maxCollisionSearch; i>=0; i--, incMod(hfree)) {
          // find a free slot
          if (0==table[hfree]) {
            table[hfree] = curr;
            return;
          }
        }
        MEDDLY_DCASSERT(table[h]);
        // full; remove entry at our slot.
#ifdef DEBUG_CT
        printf("Collision; removing CT entry ");
        FILE_output out(stdout);
        showEntry(out, table[h]);
#ifdef DEBUG_CT_SLOTS
        printf(" handle %lu in slot %lu\n", table[h], h);
#else
        printf("\n");
#endif
        fflush(stdout);
#endif
        collisions++;    

        discardAndRecycle(table[h]);
        table[h] = curr;
      }


      /** 
        Check equality.
        We advance pointer a and return the result portion if equal.
        If unequal we return 0.
      */
      static inline entry_item* equal(entry_item* a, const entry_key* key) {
        const entry_type* et = key->getET();
        MEDDLY_DCASSERT(et);
        //
        // Compare operator, if needed
        //
        if (MONOLITHIC) {
          if (et->getID() != (*a).U) return 0;
          a++;
        }
        //
        // compare #repetitions, if needed
        //
        if (et->isRepeating()) {
          if (key->numRepeats() != (*a).U) return 0;
          a++;
        }
        //
        // Compare key portion only
        //
        const entry_item* b = key->rawData();
        const unsigned klen = et->getKeySize(key->numRepeats());
        for (unsigned i=0; i<klen; i++) {
          const typeID t = et->getKeyType(i);
          switch (t) {
              case FLOAT:
                        if (a[i].F != b[i].F) return 0;
                        continue;
              case NODE:
                        if (a[i].N != b[i].N) return 0;
                        continue;
              case INTEGER:
                        if (a[i].I != b[i].I) return 0;
                        continue;
              case DOUBLE:
                        if (a[i].D != b[i].D) return 0;
                        continue;
              case GENERIC:
                        if (a[i].G != b[i].G) return 0;
                        continue;
              case LONG:
                        if (a[i].L  != b[i].L) return 0;
                        continue;
              default:
                        MEDDLY_DCASSERT(0);
          } // switch t
        } // for i
        return a + klen;
      }


      /**
          Check if the key portion of an entry equals key and should be discarded.
            @param  entry   Complete entry in CT to check.
            @param  key     Key to compare against. 
            @param  discard On output: should this entry be discarded

            @return Result portion of the entry, if they key portion matches;
                    0 if the entry does not match the key.
      */
      inline entry_item* checkEqualityAndStatus(entry_item* entry, const entry_key* key, bool &discard)
      {
        entry_item* answer = equal( CHAINED ? (entry+1) : entry, key);
        if (answer) 
        {
          //
          // Equal.
          //
          discard = isDead(answer, key->getET());
        } else {
          //
          // Not equal.
          //
          if (checkStalesOnFind) {
            discard = isStale(entry);
          } else {
            discard = false;
          } // if checkStalesOnFind
        }
        return answer;
      }

      /**
          Check if an entry is stale.
            @param  entry   Pointer to complete entry to check.
            @return         true, if the entry should be discarded.
      */
      bool isStale(const entry_item* entry) const;

      /**
          Check if a result is dead (unrecoverable).
            @param  res   Pointer to result portion of entry.
            @param  et    Entry type information.
            @return       true, if the entry should be discarded.
      */
      bool isDead(const entry_item* res, const entry_type* et) const;

      /**
          Copy a result into an entry.
      */
      inline void setResult(entry_item* respart, const entry_result &res, const entry_type* et) {
        const entry_item* resdata = res.rawData();
        memcpy(respart, resdata, res.dataLength()*sizeof(entry_item));
      }

      /// Display a chain 
      inline void showChain(output &s, size_t L) const {
        s << L;
        if (CHAINED) {
          while (L) {
#ifdef INTEGRATED_MEMMAN
            const entry_item* entry = entries+L;
#else
            const entry_item* entry = (const int*) MMAN->getChunkAddress(L);
#endif
            L = entry[0].U;
            s << "->" << L;
          } // while L
        }
        s << "\n";
      }

      /**
          Discard an entry and recycle the memory it used.
            @param  h   Handle of the entry.
      */
      void discardAndRecycle(size_t h);

      /**
          Display an entry.
          Used for debugging, and by method(s) to display the entire CT.
            @param  s         Stream to write to.
            @param  h         Handle of the entry.
      */
      void showEntry(output &s, size_t h) const;

      /// Display a key.
      void showKey(output &s, const entry_key* k) const;


    private:
      /// Global entry type.  Ignored when MONOLITHIC is true.
      const entry_type* global_et;

      /// Hash table
      size_t* table;

      /// Hash table size
      size_t tableSize;

      /// When to next expand the table
      size_t tableExpand;

      /// When to next shrink the table
      size_t tableShrink;

#ifdef INTEGRATED_MEMMAN
      /// Memory space for entries
      entry_item*  entries;
      /// Used entries
      size_t entriesSize;
      /// Memory allocated for entries
      size_t entriesAlloc;

      static const unsigned maxEntrySize = 15;
      static const unsigned maxEntryBytes = sizeof(entry_item) * maxEntrySize;

      /// freeList[i] is list of all unused i-sized entries.
      size_t* freeList;
#else
  
      memory_manager* MMAN;
#endif
  
      /// Memory statistics
      memstats mstats;

      /// How many slots to search in unchained tables
      static const unsigned maxCollisionSearch = 2;

      /// Stats: how many collisions
      size_t collisions;
  }; // class ct_none
} // namespace


// **********************************************************************
// *                                                                    *
// *                          ct_none  methods                          *
// *                                                                    *
// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
MEDDLY::ct_none<MONOLITHIC, CHAINED>::ct_none(
  const ct_initializer::settings &s, operation* op, unsigned slot)
: compute_table(s)
{
  if (MONOLITHIC) {
    MEDDLY_DCASSERT(0==op);
    MEDDLY_DCASSERT(0==slot);
  } else {
    MEDDLY_DCASSERT(op);
  }
  global_et = op ? getEntryType(op, slot) : 0;

  /*
      Initialize memory management for entries.
  */
#ifdef INTEGRATED_MEMMAN
  freeList = new size_t[1+maxEntrySize];
  for (unsigned i=0; i<=maxEntrySize; i++) {
    freeList[i] = 0;
  }
  mstats.incMemUsed( (1+maxEntrySize) * sizeof(size_t) );
  mstats.incMemAlloc( (1+maxEntrySize) * sizeof(size_t) );

  entriesAlloc = 1024;
  entriesSize = 1;    
  entries = (entry_item*) malloc(entriesAlloc * sizeof(entry_item) );
  if (0==entries) throw error(error::INSUFFICIENT_MEMORY, __FILE__, __LINE__);
  entries[0].L = 0;     // NEVER USED; set here for sanity.

  mstats.incMemUsed( entriesSize * sizeof(entry_item) );
  mstats.incMemAlloc( entriesAlloc * sizeof(entry_item) );
#else
  MEDDLY_DCASSERT(s.MMS);
  MMAN = s.MMS->initManager(sizeof(entry_item), 2, mstats);
#endif

  /*
      Initialize hash table
  */
  tableSize = 1024;
  tableExpand = CHAINED ? 4*1024 : 512;
  tableShrink = 0;
  table = (size_t*) malloc(tableSize * sizeof(size_t));
  if (0==table) throw error(error::INSUFFICIENT_MEMORY, __FILE__, __LINE__);
  for (unsigned i=0; i<tableSize; i++) table[i] = 0;

  mstats.incMemUsed(tableSize * sizeof(size_t));
  mstats.incMemAlloc(tableSize * sizeof(size_t));

  collisions = 0;
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
MEDDLY::ct_none<MONOLITHIC, CHAINED>::~ct_none()
{
  /*
    Clean up hash table
  */
  free(table);

  /*
    Clean up memory manager for entries
  */
#ifdef INTEGRATED_MEMMAN
  free(entries);
  delete[] freeList;
#else
  delete MMAN;
#endif

  /*
    Update stats: important for global usage
  */
  mstats.zeroMemUsed();
  mstats.zeroMemAlloc();
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
inline MEDDLY::compute_table::entry_item* MEDDLY::ct_none<MONOLITHIC, CHAINED>
::findEntry(entry_key* key)
{
  MEDDLY_DCASSERT(key);

  // |-----------||--------|-------------|---------|
  // | status    || active | recoverable | dead    |
  // |-----------||--------|-------------|---------|
  // | equal     || use    | use         | discard |
  // |-----------||--------|-------------|---------|
  // | not equal || keep   | discard     | discard |
  // |-----------||--------|-------------|---------|
  //
  // if (equal)
  //    if (dead) discard
  //    else use
  // else
  //    if (!active) discard
  //    else do nothing

  unsigned chain;

  entry_item* answer = 0;
  entry_item* preventry = 0;
  size_t hcurr = key->getHash() % tableSize;
  size_t curr = table[hcurr];

#ifdef DEBUG_CT_SEARCHES
  printf("Searching for CT entry ");
  FILE_output out(stdout);
  showKey(out, key);
#ifdef DEBUG_CT_SLOTS
  printf(" in hash slot %lu\n", hcurr);
#else
  printf("\n");
#endif
  fflush(stdout);
#endif

  for (chain=0; ; ) {

    if (0==curr) {
      if (CHAINED) break;
      if (++chain > maxCollisionSearch) break;
      incMod(hcurr);
      curr = table[hcurr];
      continue;
    } else {
      if (CHAINED) chain++;
    }

#ifdef INTEGRATED_MEMMAN
    entry_item* entry = entries + curr;
#else
    entry_item* entry = (entry_item*) MMAN->getChunkAddress(curr);
#endif

    bool discard;
    answer = checkEqualityAndStatus(entry, key, discard);

    if (discard) {
        //
        // Delete the entry.
        //
#ifdef DEBUG_CT
        if (answer) printf("Removing stale CT hit   ");
        else        printf("Removing stale CT entry ");
        FILE_output out(stdout);
        showEntry(out, curr);
#ifdef DEBUG_CT_SLOTS
        printf(" handle %lu in slot %lu\n", curr, hcurr);
#else
        printf("\n");
#endif
        fflush(stdout);
#endif
        if (CHAINED) {
          if (preventry) {
            preventry[0].UL = entry[0].UL;
          } else {
            table[hcurr] = entry[0].UL;
          }
        } else {
          table[hcurr] = 0;
        }

        discardAndRecycle(curr);

        if (answer) {
          answer = 0;
          // Since there can NEVER be more than one match
          // in the table, we're done!
          break;
        }
    } // if discard

    if (answer) {
        //
        // "Hit"
        //
        if (CHAINED) {
          //
          // If the matching entry is not already at the front of the list,
          // move it there
          //
          if (preventry) {
            preventry[0].UL = entry[0].UL;
            entry[0].UL = table[hcurr];
            table[hcurr] = curr;
          }
        }
#ifdef DEBUG_CT
        printf("Found CT entry ");
        FILE_output out(stdout);
        showEntry(out, curr);
#ifdef DEBUG_CT_SLOTS
        printf(" handle %lu in slot %lu\n", curr, hcurr);
#else
        printf("\n");
#endif
        fflush(stdout);
#endif
        break;
    } // if equal

    //
    // Advance
    //
    if (CHAINED) {
      curr = entry[0].UL;
      preventry = entry;
    } else {
      if (++chain > maxCollisionSearch) break;
      incMod(hcurr);
      curr = table[hcurr];
    }
    
  } // for chain

  sawSearch(chain);

  return answer;
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>
::find(entry_key *key, entry_result& res)
{
  // 
  // TBD - probably shouldn't hash the floats, doubles.
  //
  
  //
  // Hash the key
  //
  setHash(key, hash(key));

  entry_item* entry_result = findEntry(key);
  perf.pings++;

  if (entry_result) {
    perf.hits++;
    //
    // Fill res
    //
    res.reset();
    res.setValid(entry_result); // TBD THIS
  } else {
    res.setInvalid();
  }
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>::addEntry(entry_key* key, const entry_result &res)
{
  MEDDLY_DCASSERT(key);
  if (!MONOLITHIC) {
    if (key->getET() != global_et)
      throw error(error::UNKNOWN_OPERATION, __FILE__, __LINE__);
  }

  //
  // Increment cache counters for nodes in the key and result
  //
  key->cacheNodes();
  res.cacheNodes();

  size_t h = key->getHash() % tableSize;

  const entry_type* et = key->getET();
  MEDDLY_DCASSERT(et);

  //
  // Allocate an entry
  //

  const unsigned num_slots = 
    et->getKeySize(key->numRepeats()) + 
    + et->getResultSize()
    + (CHAINED ? 1 : 0)
    + (MONOLITHIC ? 1 : 0)
    + (et->isRepeating() ? 1 : 0)
  ;
  size_t curr = newEntry(num_slots);

#ifdef INTEGRATED_MEMMAN
  entry_item* entry = entries + curr;
#else
  entry_item* entry = (entry_item*) MMAN->getChunkAddress(curr);
#endif

  //
  // Copy into the entry
  //
  entry_item* key_portion = entry + (CHAINED ? 1 : 0);
  if (MONOLITHIC) {
    (*key_portion).U = et->getID();
    key_portion++;
  }
  if (et->isRepeating()) {
    (*key_portion).U = key->numRepeats();
    key_portion++;
  }
  const unsigned key_slots = et->getKeySize(key->numRepeats());
  memcpy(key_portion, key->rawData(), key_slots * sizeof(entry_item));
  entry_item* res_portion = key_portion + key_slots;
  setResult(res_portion, res, et);

  //
  // Recycle key
  //
  recycle(key);


  //
  // Add entry to CT
  //
  if (CHAINED) {
    // Add this to front of chain
    entry[0].UL = table[h];
    table[h] = curr;
  } else {
    setTable(h, curr);
  }

#ifdef DEBUG_CT
  printf("Added CT entry ");
  FILE_output out(stdout);
  showEntry(out, curr);
#ifdef DEBUG_CT_SLOTS
  printf(" handle %lu in slot %lu (%u slots long)\n", curr, h, num_slots);
#else
  printf("\n");
#endif
  fflush(stdout);
#endif

  if (perf.numEntries < tableExpand) return;

  //
  // Time to GC and maybe resize the table
  //

#ifdef DEBUG_SLOW
  fprintf(stdout, "Running GC in compute table (size %d, entries %u)\n", 
    tableSize, perf.numEntries
  );
#endif

  size_t list = 0;
  if (CHAINED) {
    list = convertToList(checkStalesOnResize);
    if (perf.numEntries < tableSize) {
      listToTable(list);
#ifdef DEBUG_SLOW
      fprintf(stdout, "Done CT GC, no resizing (now entries %u)\n", perf.numEntries);
#endif
      return;
    }
  } else {
    scanForStales();
    if (perf.numEntries < tableExpand / 4) {
#ifdef DEBUG_SLOW
      fprintf(stdout, "Done CT GC, no resizing (now entries %u)\n", perf.numEntries);
#endif
      return;
    }
  }

  size_t newsize = tableSize * 2;
  if (newsize > maxSize) newsize = maxSize;

  if (CHAINED) {
    if (newsize != tableSize) {
      //
      // Enlarge table
      //
    
      size_t* newt = (size_t*) realloc(table, newsize * sizeof(size_t));
      if (0==newt) {
        throw error(error::INSUFFICIENT_MEMORY, __FILE__, __LINE__);
      }

      for (size_t i=tableSize; i<newsize; i++) newt[i] = 0;

      MEDDLY_DCASSERT(newsize > tableSize);
      mstats.incMemUsed( (newsize - tableSize) * sizeof(size_t) );
      mstats.incMemAlloc( (newsize - tableSize) * sizeof(size_t) );

      table = newt;
      tableSize = newsize;
    }

    if (tableSize == maxSize) {
      tableExpand = std::numeric_limits<int>::max();
    } else {
      tableExpand = 4*tableSize;
    }
    tableShrink = tableSize / 2;

    listToTable(list);
  } else {  // not CHAINED
    if (newsize != tableSize) {
      //
      // Enlarge table
      //
      size_t* oldT = table;
      size_t oldSize = tableSize;
      tableSize = newsize;
      table = (size_t*) malloc(newsize * sizeof(size_t));
      if (0==table) {
        table = oldT;
        tableSize = oldSize;
        throw error(error::INSUFFICIENT_MEMORY, __FILE__, __LINE__);
      }
      for (size_t i=0; i<newsize; i++) table[i] = 0;

      mstats.incMemUsed(newsize * sizeof(size_t));
      mstats.incMemAlloc(newsize * sizeof(size_t));

      rehashTable(oldT, oldSize);
      free(oldT);

      mstats.decMemUsed(oldSize * sizeof(size_t));
      mstats.decMemAlloc(oldSize * sizeof(size_t));

      if (tableSize == maxSize) {
        tableExpand = std::numeric_limits<int>::max();
      } else {
        tableExpand = tableSize / 2;
      }
      tableShrink = tableSize / 8;
    }
  } // if CHAINED

#ifdef DEBUG_SLOW
  fprintf(stdout, "CT enlarged to size %lu\n", tableSize);
#endif

}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>::updateEntry(entry_key* key, const entry_result &res)
{
  MEDDLY_DCASSERT(key->getET()->isResultUpdatable());
  entry_item* entry_result = findEntry(key);
  if (!entry_result) {
    throw error(error::INVALID_ARGUMENT, __FILE__, __LINE__);
  }

  //
  // decrement cache counters for old result,
  //

  const entry_type* et = key->getET();
  for (unsigned i=0; i<et->getResultSize(); i++) {
#ifdef DEVELOPMENT_CODE
    typeID t;
    expert_forest* f;
    et->getResultType(i, t, f);
#else
    expert_forest* f = et->getResultForest(i);
#endif
    if (f) {
      MEDDLY_DCASSERT(NODE==t);
      f->uncacheNode( entry_result[i].N );
    }
  } // for i

  //
  // increment cache counters for new result.
  //
  res.cacheNodes();

  //
  // Overwrite result
  //
  setResult(entry_result, res, key->getET());
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>::removeStales()
{
#ifdef DEBUG_REMOVESTALES
  fprintf(stdout, "Removing stales in CT (size %lu, entries %lu)\n", 
        tableSize, perf.numEntries
  );
#endif

  if (CHAINED) {

    //
    // Chained 
    //
    size_t list = convertToList(true);

    if (perf.numEntries < tableShrink) {
      //
      // Time to shrink table
      //
      size_t newsize = tableSize / 2;
      if (newsize < 1024) newsize = 1024;
      size_t* newt = (size_t*) realloc(table, newsize * sizeof(size_t));
      if (0==newt) {
        throw error(error::INSUFFICIENT_MEMORY, __FILE__, __LINE__); 
      }

      MEDDLY_DCASSERT(tableSize > newsize);
      mstats.decMemUsed( (tableSize - newsize) * sizeof(size_t) );
      mstats.decMemAlloc( (tableSize - newsize) * sizeof(size_t) );

      table = newt;
      tableSize = newsize;
      tableExpand = 4*tableSize;
      if (1024 == tableSize) {
        tableShrink = 0;
      } else {
        tableShrink = tableSize / 2;
      }
    }

    listToTable(list);

  } else {

    //
    // Unchained
    //

    scanForStales();

    if (perf.numEntries < tableShrink) {
      //
      // Time to shrink table
      //
      size_t newsize = tableSize / 2;
      if (newsize < 1024) newsize = 1024;
      if (newsize < tableSize) {
          size_t* oldT = table;
          size_t oldSize = tableSize;
          tableSize = newsize;
          table = (size_t*) malloc(newsize * sizeof(size_t));
          if (0==table) {
            table = oldT;
            tableSize = oldSize;
            throw error(error::INSUFFICIENT_MEMORY, __FILE__, __LINE__);
          }
          for (size_t i=0; i<newsize; i++) table[i] = 0;
          mstats.incMemUsed(newsize * sizeof(size_t));
          mstats.incMemAlloc(newsize * sizeof(size_t));
    
          rehashTable(oldT, oldSize);
          free(oldT);
      
          mstats.decMemUsed(oldSize * sizeof(size_t));
          mstats.decMemAlloc(oldSize * sizeof(size_t));
    
          tableExpand = tableSize / 2;
          if (1024 == tableSize) {
            tableShrink = 0;
          } else {
            tableShrink = tableSize / 8;
          }
      } // if different size
    }
  } // if CHAINED
    
#ifdef DEBUG_REMOVESTALES
  fprintf(stdout, "Done removing CT stales (size %lu, entries %lu)\n", 
    tableSize, perf.numEntries
  );
#ifdef DEBUG_REMOVESTALES_DETAILS
  FILE_output out(stderr);
  out << "CT after removing stales:\n";
  show(out, 9);
#endif
  fflush(stdout);
#endif
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>::removeAll()
{
#ifdef DEBUG_REMOVESTALES
  fprintf(stdout, "Removing all CT entries\n");
#endif
  for (unsigned i=0; i<tableSize; i++) {
    while (table[i]) {
      int curr = table[i];
#ifdef INTEGRATED_MEMMAN
      const entry_item* entry = entries + curr;
#else
      const entry_item* entry = (const int*) MMAN->getChunkAddress(curr);
#endif
      if (CHAINED) {
        table[i] = entry[0].UL;
      } else {
        table[i] = 0;
      }
      discardAndRecycle(curr);
    } // while
  } // for i
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>
::show(output &s, int verbLevel)
{
  if (verbLevel < 1) return;

  if (MONOLITHIC) {
    s << "Monolithic compute table\n";
  } else {
    s << "Compute table for " << global_et->getName() << " (index " 
      << long(global_et->getID()) << ")\n";
  }

  s.put("", 6);
  s << "Current CT memory   :\t" << mstats.getMemUsed() << " bytes\n";
  s.put("", 6);
  s << "Peak    CT memory   :\t" << mstats.getPeakMemUsed() << " bytes\n";
  s.put("", 6);
  s << "Current CT alloc'd  :\t" << mstats.getMemAlloc() << " bytes\n";
  s.put("", 6);
  s << "Peak    CT alloc'd  :\t" << mstats.getPeakMemAlloc() << " bytes\n";
  if (!CHAINED) {
    s.put("", 6);
    s << "Collisions          :\t" << long(collisions) << "\n";
  }
  s.put("", 6);
  s << "Hash table size     :\t" << long(tableSize) << "\n";
  s.put("", 6);
  s << "Number of entries   :\t" << long(perf.numEntries) << "\n";

  if (--verbLevel < 1) return;

  s.put("", 6);
  s << "Pings               :\t" << long(perf.pings) << "\n";
  s.put("", 6);
  s << "Hits                :\t" << long(perf.hits) << "\n";

  if (--verbLevel < 1) return;

  s.put("", 6);
  s << "Search length histogram:\n";
  for (int i=0; i<stats::searchHistogramSize; i++) {
    if (perf.searchHistogram[i]) {
      s.put("", 10);
      s.put(long(i), 3);
      s << ": " << long(perf.searchHistogram[i]) << "\n";
    }
  }
  if (perf.numLargeSearches) {
    s.put("", 6);
    s << "Searches longer than " << long(stats::searchHistogramSize-1)
      << ": " << long(perf.numLargeSearches) << "\n";
  }
  s.put("", 6);
  s << "Max search length: " << long(perf.maxSearchLength) << "\n";


  if (--verbLevel < 1) return;

  s << "Hash table:\n";

  for (unsigned i=0; i<tableSize; i++) {
    if (0==table[i]) continue;
    s << "table[";
    s.put(long(i), 9);
    s << "]: ";
    showChain(s, table[i]);
  }

  if (--verbLevel < 1) return;

  s << "\nHash table nodes:\n";
  
  for (unsigned i=0; i<tableSize; i++) {
    int curr = table[i];
    while (curr) {
      s << "\tNode ";
      s.put(long(curr), 9);
      s << ":  ";
      showEntry(s, curr);
      s.put('\n');
      if (CHAINED) {
#ifdef INTEGRATED_MEMMAN
        curr = entries[curr].UL;
#else
        const int* entry = (const int*) MMAN->getChunkAddress(curr);
        curr = entry[0].UL;
#endif
      } else {
        curr = 0;
      }
    } // while curr
  } // for i
  s.put('\n');

  if (--verbLevel < 1) return;

#ifdef INTEGRATED_MEMMAN
  if (0==freeList) {
    s << "Free: null\n";
  } else {
    for (int i=1; i<=maxEntrySize; i++) {
      if (freeList[i]) {
        s << "freeList[" << i << "]: ";

        int L = freeList[i];
        s << L;
        while (L) {
          L = entries[L].UL;
          s << "->" << L;
        }
        s << "\n";
      }
    }
  }

  if (0==entries) {
    s << "Entries: null\n";
  } else {
    s << "Entries: [" << long(entries[0].L);
    for (int i=1; i<entriesSize; i++) {
      s << ", " << long(entries[i].L);
    }
    s << "]\n";
  }
#else
  if (MMAN) MMAN->dumpInternal(s);
#endif

}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
size_t MEDDLY::ct_none<MONOLITHIC, CHAINED>::convertToList(bool removeStales)
{
  MEDDLY_DCASSERT(CHAINED);
  // const int SHIFT = (MONOLITHIC ? 1 : 0) + (CHAINED ? 1 : 0);

  size_t list = 0;
  for (size_t i=0; i<tableSize; i++) {
    while (table[i]) {
      size_t curr = table[i];
#ifdef INTEGRATED_MEMMAN
      entry_item* entry = entries + curr;
#else
      entry_item* entry = (entry_item*) MMAN->getChunkAddress(curr);
#endif
      table[i] = entry[0].UL;
      if (removeStales && isStale(entry)) {

#ifdef DEBUG_TABLE2LIST
          printf("\tstale ");
          FILE_output out(stdout);
          showEntry(out, curr);
          printf(" (handle %lu table slot %lu)\n", curr, i);
#endif

          discardAndRecycle(curr);
          continue;
        // }
      } // if removeStales
      //
      // Not stale, move to list
      //
#ifdef DEBUG_TABLE2LIST
      printf("\tkeep  ");
      FILE_output out(stdout);
      showEntry(out, curr);
      printf(" (handle %lu table slot %lu)\n", curr, i);
#endif
      entry[0].UL = list;
      list = curr;
    } // while
  } // for i
#ifdef DEBUG_TABLE2LIST
  printf("Built list: ");
  showChain(stdout, list);
#endif
  return list;
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>::listToTable(size_t L)
{
  MEDDLY_DCASSERT(CHAINED);
#ifdef DEBUG_LIST2TABLE
  printf("Recovering  list");
  // showChain(stdout, L);
#endif
  while (L) {
#ifdef INTEGRATED_MEMMAN
    entry_item* entry = entries+L;
#else
    entry_item* entry = (entry_item*) MMAN->getChunkAddress(L);
#endif
    const size_t curr = L;
    L = entry[0].UL;

    const entry_type* et = MONOLITHIC
      ?   getEntryType(entry[1].U)
      :   global_et;
    MEDDLY_DCASSERT(et);

    const unsigned h = hash(et, entry + 1) % tableSize;
    entry[0].UL = table[h];
    table[h] = curr;
#ifdef DEBUG_LIST2TABLE
    printf("\tsave  ");
    FILE_output out(stdout);
    showEntry(out, curr);
    printf(" (handle %lu table slot %lu hashlength %u)\n", curr, h, hashlength);
#endif
  }
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>::scanForStales()
{
  MEDDLY_DCASSERT(!CHAINED);
  for (size_t i=0; i<tableSize; i++) {
    if (0==table[i]) continue;

#ifdef INTEGRATED_MEMMAN
    const entry_item* entry = entries + table[i];
#else
    const entry_item* entry = (const entry_item*) MMAN->getChunkAddress(table[i]);
#endif

    if (isStale(entry)) {

#ifdef DEBUG_CT
      printf("Removing CT stale entry ");
      FILE_output out(stdout);
      showEntry(out, table[i]);
#ifdef DEBUG_CT_SLOTS
      printf(" in table slot %lu\n", i);
#else
      printf("\n");
#endif
#endif  

      discardAndRecycle(table[i]);
      table[i] = 0;

    } // if isStale
  } // for i
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>::rehashTable(const size_t* oldT, size_t oldS)
{
#ifdef DEBUG_REHASH
  printf("Rebuilding hash table\n");
#endif
  MEDDLY_DCASSERT(!CHAINED);
  for (size_t i=0; i<oldS; i++) {
    size_t curr = oldT[i];
    if (0==curr) continue;
#ifdef INTEGRATED_MEMMAN
    const entry_item* entry = entries + curr;
#else
    const entry_item* entry = (const entry_item*) MMAN->getChunkAddress(curr);
#endif

    const entry_type* et = MONOLITHIC
      ?   getEntryType(entry[0].UL)
      :   global_et;
    MEDDLY_DCASSERT(et);

    size_t h = hash(et, entry) % tableSize;
    setTable(h, curr);

#ifdef DEBUG_REHASH
    printf("\trehash  ");
    FILE_output out(stdout);
    showEntry(out, curr);
    printf(" (handle %lu table slot %lu hashlength %u)\n", curr, h, hashlength);
#endif
  }
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
MEDDLY::node_address MEDDLY::ct_none<MONOLITHIC, CHAINED>
::newEntry(unsigned size)
{
#ifdef INTEGRATED_MEMMAN
  // check free list
  if (size > maxEntrySize) {
    fprintf(stderr, "MEDDLY error: request for compute table entry larger than max size\n");
    throw error(error::MISCELLANEOUS, __FILE__, __LINE__);  // best we can do
  }
  if (size<1) return 0;
  perf.numEntries++;
  if (freeList[size]) {
    node_address h = freeList[size];
    freeList[size] = entries[h].UL;
#ifdef DEBUG_CTALLOC
    fprintf(stderr, "Re-used entry %ld size %d\n", h, size);
#endif
    mstats.incMemUsed( size * sizeof(entry_item) );
    return h;
  }
  if (entriesSize + size > entriesAlloc) {
    // Expand by a factor of 1.5
    unsigned neA = entriesAlloc + (entriesAlloc/2);
    entry_item* ne = (entry_item*) realloc(entries, neA * sizeof(entry_item));
    if (0==ne) {
      fprintf(stderr,
          "Error in allocating array of size %lu at %s, line %d\n",
          neA * sizeof(int), __FILE__, __LINE__);
      throw error(error::INSUFFICIENT_MEMORY, __FILE__, __LINE__);
    }
    mstats.incMemAlloc( (entriesAlloc / 2) * sizeof(entry_item) );
    entries = ne;
    entriesAlloc = neA;
  }
  MEDDLY_DCASSERT(entriesSize + size <= entriesAlloc);
  node_address h = entriesSize;
  entriesSize += size;
#ifdef DEBUG_CTALLOC
  fprintf(stderr, "New entry %lu size %u\n", h, size);
#endif
  mstats.incMemUsed( size * sizeof(entry_item) );
  return h;

#else
  perf.numEntries++;
  size_t the_size = size;
  return MMAN->requestChunk(the_size);
#endif
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
unsigned MEDDLY::ct_none<MONOLITHIC, CHAINED>
::hash(const entry_key* key)
{
  const entry_type* et = key->getET();
  MEDDLY_DCASSERT(et);

  hash_stream H;
  H.start();

  //
  // Operator, if needed
  //
  if (MONOLITHIC) {
    H.push(et->getID());
  }
  //
  // #repetitions, if needed
  //
  if (et->isRepeating()) {
    H.push(key->numRepeats());
  }

  //
  // Hash key portion only
  //

  const entry_item* entry = key->rawData();
  const unsigned klen = et->getKeySize(key->numRepeats());
  for (unsigned i=0; i<klen; i++) {    // i initialized earlier
    const typeID t = et->getKeyType(i);
    switch (t) {
        case FLOAT:
                        MEDDLY_DCASSERT(sizeof(entry[i].F) == sizeof(entry[i].U));
        case NODE:
                        MEDDLY_DCASSERT(sizeof(entry[i].N) == sizeof(entry[i].U));
        case INTEGER:
                        MEDDLY_DCASSERT(sizeof(entry[i].I) == sizeof(entry[i].U));
                        H.push(entry[i].U);
                        continue;

        case DOUBLE:
                        MEDDLY_DCASSERT(sizeof(entry[i].D) == sizeof(entry[i].L));
        case GENERIC:
                        MEDDLY_DCASSERT(sizeof(entry[i].G) == sizeof(entry[i].L));
        case LONG:      
                        {
                          unsigned* hack = (unsigned*) (& (entry[i].L));
                          H.push(hack[0], hack[1]);
                        }
                        continue;
        default:
                        MEDDLY_DCASSERT(0);
    } // switch t
  } // for i

  return H.finish();
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
unsigned MEDDLY::ct_none<MONOLITHIC, CHAINED>
::hash(const entry_type* et, const entry_item* entry)
{
  hash_stream H;
  H.start();
  //
  // Operator, if needed
  //
  if (MONOLITHIC) {
    MEDDLY_DCASSERT(et->getID() == entry[0].U);
    H.push(et->getID());
    entry++;
  }
  //
  // #repetitions, if needed
  //
  unsigned reps = 0;
  if (et->isRepeating()) {
    H.push(entry[0].U);
    reps = entry[0].U;
    entry++;
  }

  //
  // Hash key portion only
  //

  const unsigned klen = et->getKeySize(reps);
  for (unsigned i=0; i<klen; i++) { 
    const typeID t = et->getKeyType(i);
    switch (t) {
        case FLOAT:
                        MEDDLY_DCASSERT(sizeof(entry[i].F) == sizeof(entry[i].U));
        case NODE:
                        MEDDLY_DCASSERT(sizeof(entry[i].N) == sizeof(entry[i].U));
        case INTEGER:
                        MEDDLY_DCASSERT(sizeof(entry[i].I) == sizeof(entry[i].U));
                        H.push(entry[i].U);
                        continue;

        case DOUBLE:
                        MEDDLY_DCASSERT(sizeof(entry[i].D) == sizeof(entry[i].L));
        case GENERIC:
                        MEDDLY_DCASSERT(sizeof(entry[i].G) == sizeof(entry[i].L));
        case LONG:      {
                          unsigned* hack = (unsigned*) (& (entry[i].L));
                          H.push(hack[0], hack[1]);
                        }
                        continue;
        default:
                        MEDDLY_DCASSERT(0);
    } // switch t
  } // for i

  return H.finish();
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
bool MEDDLY::ct_none<MONOLITHIC, CHAINED> 
::isStale(const entry_item* entry) const
{
  const int SHIFT = (MONOLITHIC ? 1 : 0) + (CHAINED ? 1 : 0);

  //
  // Check entire entry for staleness
  //
  const entry_type* et = MONOLITHIC
      ?   getEntryType(entry[CHAINED?1:0].U)
      :   global_et;
  MEDDLY_DCASSERT(et);

  if (et->isMarkedForDeletion()) return true;

  entry += SHIFT;
  const unsigned reps = (et->isRepeating()) ? (*entry++).U : 0;
  const unsigned klen = et->getKeySize(reps);

  //
  // Key portion
  //
  for (unsigned i=0; i<klen; i++) {
#ifdef DEVELOPMENT_CODE
    typeID t;
    expert_forest* f;
    et->getKeyType(i, t, f);
#else
    expert_forest* f = et->getKeyForest(i);
#endif
    if (f) {
      MEDDLY_DCASSERT(NODE==t);
      if (MEDDLY::forest::ACTIVE != f->getNodeStatus(entry[i].N)) {
        return true;
      }
    }
  } // for i
  entry += klen;

  // 
  // Result portion
  //
  for (unsigned i=0; i<et->getResultSize(); i++) {
#ifdef DEVELOPMENT_CODE
    typeID t;
    expert_forest* f;
    et->getResultType(i, t, f);
#else
    expert_forest* f = et->getResultForest(i);
#endif
    if (f) {
      MEDDLY_DCASSERT(NODE==t);
      if (MEDDLY::forest::ACTIVE != f->getNodeStatus(entry[i].N)) {
        return true;
      }
    }
  } // for i

  return false;
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
bool MEDDLY::ct_none<MONOLITHIC, CHAINED> 
::isDead(const entry_item* result, const entry_type* et) const
{
  MEDDLY_DCASSERT(et);
  MEDDLY_DCASSERT(result);
  //
  // Check result portion for dead nodes - cannot use result in that case
  //
  for (unsigned i=0; i<et->getResultSize(); i++) {
#ifdef DEVELOPMENT_CODE
    typeID t;
    expert_forest* f;
    et->getResultType(i, t, f);
#else
    expert_forest* f = et->getResultForest(i);
#endif
    if (f) {
      MEDDLY_DCASSERT(NODE == t);
      if (MEDDLY::forest::DEAD == f->getNodeStatus(result[i].N)) {
        return true;
      }
    }
  } // for i
  return false;
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>
::discardAndRecycle(size_t h)
{
  const int SHIFT = (MONOLITHIC ? 1 : 0) + (CHAINED ? 1 : 0);

#ifdef INTEGRATED_MEMMAN
  entry_item* entry = entries + h;
#else
  entry_item* entry = (entry_item*) MMAN->getChunkAddress(table[h]);
#endif

  unsigned slots;

  //
  // Discard.
  // For the portion of the entry that are nodes,
  // we need to notify the forest to decrement
  // the CT counter.
  //

  const entry_type* et = MONOLITHIC
    ?   getEntryType(entry[CHAINED ? 1 : 0].U)
    :   global_et;
  MEDDLY_DCASSERT(et);

  const entry_item* ptr = entry + SHIFT;
  unsigned reps;
  if (et->isRepeating()) {
    reps = (*ptr).U;
    ptr++;
  } else {
    reps = 0;
  }

  //
  // Key portion
  //
  const unsigned stop = et->getKeySize(reps);
  for (unsigned i=0; i<stop; i++) {
    typeID t;
    expert_forest* f;
    et->getKeyType(i, t, f);
    if (f) {
      MEDDLY_DCASSERT(NODE == t);
      f->uncacheNode( ptr[i].N );
      continue;
    }
    if (GENERIC == t) {
      delete ptr[i].G;
      continue;
    }
    MEDDLY_DCASSERT( t != NODE );
  } // for i
  ptr += stop;

  //
  // Result portion
  //
  for (unsigned i=0; i<et->getResultSize(); i++) {
    typeID t;
    expert_forest* f;
    et->getResultType(i, t, f);
    if (f) {
      MEDDLY_DCASSERT(NODE == t);
      f->uncacheNode( ptr[i].N );
      continue;
    }
    if (GENERIC == t) {
      delete ptr[i].G;
      continue;
    }
    MEDDLY_DCASSERT( t != NODE );
  } // for i
  
  slots = SHIFT + (et->isRepeating() ? 1 : 0);
  slots += stop;
  slots += et->getResultSize();

  //
  // Recycle
  //
#ifdef DEBUG_CTALLOC
  fprintf(stderr, "Recycling entry %lu size %u\n", h, slots);
#endif
#ifdef INTEGRATED_MEMMAN
  entries[h].UL = freeList[slots];
  freeList[slots] = h;
  mstats.decMemUsed( slots * sizeof(entry_item) );
#else
  MMAN->recycleChunk(h, slots);
#endif
  perf.numEntries--;
}

// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>
::showEntry(output &s, size_t h) const
{
#ifdef INTEGRATED_MEMMAN
  const entry_item* entry = entries+h;
#else
  const entry_item* entry = (const entry_item*) MMAN->getChunkAddress(h);
#endif

  const entry_type* et = MONOLITHIC
    ?   getEntryType(entry[CHAINED?1:0].U)
    :   global_et;
  MEDDLY_DCASSERT(et);

  const entry_item* ptr = entry + (MONOLITHIC ? 1 : 0) + (CHAINED ? 1 : 0);
  unsigned reps;
  if (et->isRepeating()) {
    reps = (*ptr).U;
    ptr++;
  } else {
    reps = 0;
  }
  s << "[" << et->getName() << "(";
  unsigned stop = et->getKeySize(reps);
  for (unsigned i=0; i<stop; i++) {
      if (i) s << ", ";
      switch (et->getKeyType(i)) {
        case NODE:
                        s.put(long(ptr[i].N));
                        break;
        case INTEGER:
                        s.put(long(ptr[i].I));
                        break;
        case LONG:
                        s.put(ptr[i].L);
                        break;
        case FLOAT:
                        s.put(ptr[i].F, 0, 0, 'e');
                        break;
        case DOUBLE:
                        s.put(ptr[i].D, 0, 0, 'e');
                        break;
        case GENERIC:
                        s.put_hex((size_t)ptr[i].G);
                        break;
        default:
                        MEDDLY_DCASSERT(0);
      } // switch et->getKeyType()
  } // for i
  ptr += stop;
  s << "): ";
  for (unsigned i=0; i<et->getResultSize(); i++) {
      if (i) s << ", ";
      switch (et->getResultType(i)) {
        case NODE:
                        s.put(long(ptr[i].N));
                        break;
        case INTEGER:
                        s.put(long(ptr[i].I));
                        break;
        case LONG:
                        s.put(ptr[i].L);
                        s << "(L)";
                        break;
        case FLOAT:
                        s.put(ptr[i].F, 0, 0, 'e');
                        break;
        case DOUBLE:
                        s.put(ptr[i].D, 0, 0, 'e');
                        break;
                            
        case GENERIC:
                        s.put_hex((size_t)ptr[i].G);
                        break;
        default:
                        MEDDLY_DCASSERT(0);
      } // switch et->getResultType()
  } // for i
  s << "]";
}


// **********************************************************************

template <bool MONOLITHIC, bool CHAINED>
void MEDDLY::ct_none<MONOLITHIC, CHAINED>
::showKey(output &s, const entry_key* key) const
{
  MEDDLY_DCASSERT(key);

  const entry_type* et = key->getET();
  MEDDLY_DCASSERT(et);

  unsigned reps = key->numRepeats();
  s << "[" << et->getName() << "(";
  unsigned stop = et->getKeySize(reps);

  for (unsigned i=0; i<stop; i++) {
      entry_item item = key->rawData()[i];
      if (i) s << ", ";
      switch (et->getKeyType(i)) {
        case NODE:
                        s.put(long(item.N));
                        break;
        case INTEGER:
                        s.put(long(item.I));
                        break;
        case LONG:
                        s.put(item.L);
                        break;
        case FLOAT:
                        s.put(item.F);
                        break;
        case DOUBLE:
                        s.put(item.D);
                        break;
        case GENERIC:
                        s.put_hex((size_t)item.G);
                        break;
        default:
                        MEDDLY_DCASSERT(0);
      } // switch et->getKeyType()
  } // for i
  s << "): ?]";
}


#endif  // include guard

