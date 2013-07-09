
// $Id$

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



// TODO: Testing

#include "hm_array.h"


#define MERGE_AND_SPLIT_HOLES
// #define MEMORY_TRACE
// #define DEEP_MEMORY_TRACE


// ******************************************************************
// *                                                                *
// *                                                                *
// *                        hm_array methods                        *
// *                                                                *
// *                                                                *
// ******************************************************************

MEDDLY::hm_array::hm_array(node_storage* p) : holeman(4, p)
{
  large_holes = 0;
  for (int i=0; i<LARGE_SIZE; i++) {
    small_holes[i] = 0;
  }
#ifdef MEASURE_LARGE_HOLE_STATS
  num_large_hole_traversals = 0;
  count_large_hole_visits = 0;
#endif
}

// ******************************************************************

MEDDLY::hm_array::~hm_array()
{
}

// ******************************************************************

MEDDLY::node_address MEDDLY::hm_array::requestChunk(int slots)
{
#ifdef MEMORY_TRACE
  printf("Requesting %d slots\n", slots);
#endif
  node_handle found = 0;
  // Try for an exact fit first
  if (slots < LARGE_SIZE) {
    if (small_holes[slots]) {
      found = small_holes[slots];
      listRemove(small_holes[slots], found);
      MEDDLY_DCASSERT(data[found] = -slots);
    }
  }
  // Try the large hole list and check for a hole large enough
  if (!found) {
#ifdef MEASURE_LARGE_HOLE_STATS
    num_large_hole_traversals++;
#endif
    node_handle curr = large_holes;
    for (node_handle curr = large_holes; curr; curr = Next(curr)) {
#ifdef MEASURE_LARGE_HOLE_STATS
      count_large_hole_visits++;
#endif
      if (-data[curr] >= slots) {
        // we have a large enough hole, grab it
        found = curr;
        // remove the hole from the list
        listRemove(large_holes, found);
        break;
      }
    }
  }

  if (found) {
    // We found a hole to recycle.
#ifdef MEMORY_TRACE
    printf("Removed hole %d\n", found);
#ifdef DEEP_MEMORY_TRACE
    getParent()->dumpInternal(stdout);
#else 
    dumpHole(stdout, found);
#endif
#endif
    // Sanity check:
    MEDDLY_DCASSERT(slots <= -data[found]);
    
    // Update stats
    useHole(-data[found]);

    node_handle newhole = found + slots;
    node_handle newsize = -(data[found]) - slots;
    data[found] = -slots;
    if (newsize > 0) {
      // Save the leftovers - make a new hole!
      data[newhole] = -newsize;
      data[newhole + newsize - 1] = -newsize;
      newHole(newsize);
      insertHole(newhole);
    }
    return found;
  }
  
  // 
  // Still here?  We couldn't recycle a node.
  // 
  return allocFromEnd(slots);
}

// ******************************************************************

void MEDDLY::hm_array::recycleChunk(node_address addr, int slots)
{
#ifdef MEMORY_TRACE
  printf("Calling recycleChunk(%ld, %d)\n", long(addr), slots);
#endif

  decMemUsed(slots * sizeof(node_handle));

  newHole(slots);
  data[addr] = data[addr+slots-1] = -slots;

  if (!getForest()->getPolicies().recycleNodeStorageHoles) return;

  // Check for a hole to the left
#ifdef MERGE_AND_SPLIT_HOLES
  if (data[addr-1] < 0) {
    // Merge!
#ifdef MEMORY_TRACE
    printf("Left merging\n");
#endif
    // find the left hole address
    node_handle lefthole = addr + data[addr-1];
    MEDDLY_DCASSERT(data[lefthole] == data[addr-1]);
    useHole(slots);
    useHole(-data[lefthole]);

    // remove the left hole
    removeHole(lefthole);

    // merge with us
    slots += (-data[lefthole]);
    addr = lefthole;
    data[addr] = data[addr+slots-1] = -slots;
    newHole(slots);
  }
#endif

  // if addr is the last hole, absorb into free part of array
  MEDDLY_DCASSERT(addr + slots - 1 <= lastSlot());
  if (addr+slots-1 == lastSlot()) {
    releaseToEnd(addr, slots);
    return;
  }

  // Still here? Wasn't the last hole.

#ifdef MERGE_AND_SPLIT_HOLES
  // Check for a hole to the right
  if (data[addr+slots]<0) {
    // Merge!
#ifdef MEMORY_TRACE
    printf("Right merging\n");
#endif
    // find the right hole address
    node_handle righthole = addr+slots;

    useHole(slots);
    useHole(-data[righthole]);

    // remove the right hole
    removeHole(righthole);
    
    // merge with us
    slots += (-data[righthole]);
    data[addr] = data[addr+slots-1] = -slots;
    newHole(slots);
  }
#endif

  // Add hole to the proper list
  insertHole(addr);

#ifdef MEMORY_TRACE
  printf("Made Hole %ld\n", long(addr));
#ifdef DEEP_MEMORY_TRACE
  getParent()->dumpInternal(stdout);
#else
  dumpHole(stdout, addr);
#endif
#endif
}

// ******************************************************************

void MEDDLY::hm_array::dumpInternalInfo(FILE* s) const
{
  fprintf(s, "Last slot used: %ld\n", long(lastSlot()));
  fprintf(s, "Total hole slots: %ld\n", holeSlots());
  fprintf(s, "small_holes: (");
  bool printed = false;
  for (int i=0; i<LARGE_SIZE; i++) if (small_holes[i]) {
    if (printed) fprintf(s, ", ");
    fprintf(s, "%d:%d", i, small_holes[i]);
    printed = true;
  }
  fprintf(s, ")\n");
  fprintf(s, "large_holes: %ld\n", long(large_holes));
}

// ******************************************************************

void MEDDLY::hm_array::dumpHole(FILE* s, node_address a) const
{
  MEDDLY_DCASSERT(data);
  MEDDLY_CHECK_RANGE(1, a, lastSlot());
  long aN = chunkAfterHole(a)-1;
  fprintf(s, "[%ld, p: %ld, n: %ld, ..., %ld]\n", 
      long(data[a]), long(data[a+1]), long(data[a+2]), long(data[aN])
  );
}

// ******************************************************************

void MEDDLY::hm_array
::reportStats(FILE* s, const char* pad, unsigned flags) const
{
  static unsigned HOLE_MANAGER =
    expert_forest::HOLE_MANAGER_STATS | expert_forest::HOLE_MANAGER_DETAILED;

  if (! (flags & HOLE_MANAGER)) return;

  fprintf(s, "%sStats for array of lists hole management\n", pad);

  holeman::reportStats(s, pad, flags);

#ifdef MEASURE_LARGE_HOLE_STATS
  if (flags & expert_forest::HOLE_MANAGER_STATS) {
    fprintf(s, "%s    #traversals large_holes: %ld\n", pad, num_large_hole_traversals);
    if (num_large_hole_traversals) {
      fprintf(s, "%s    total traversal cost: %ld\n", pad, count_large_hole_visits);
      double avg = count_large_hole_visits;
      avg /= num_large_hole_traversals;
      fprintf(s, "%s    Avg cost per traversal : %lf\n", pad, avg);
    }
  }
#endif

  if (flags & expert_forest::HOLE_MANAGER_DETAILED) {
    fprintf(s, "%s    Length of non-empty chains:\n", pad);
    for (int i=0; i<LARGE_SIZE; i++) {
      long L = listLength(small_holes[i]);
      if (L) {
        fprintf(s, "%s\tsize %3d: %ld\n", pad, i, L);
      }
    }
    long LL = listLength(large_holes);
    if (LL) fprintf(s, "%s\tlarge   : %ld\n", pad, listLength(large_holes));
  }

}

// ******************************************************************

void MEDDLY::hm_array::clearHolesAndShrink(node_address new_last, bool shrink)
{
  holeman::clearHolesAndShrink(new_last, shrink);

  // set up hole pointers and such
  large_holes = 0;
  for (int i=0; i<LARGE_SIZE; i++) small_holes[i] = 0;
}

