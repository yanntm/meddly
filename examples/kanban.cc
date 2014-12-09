
// $Id$

/*
    Meddly: Multi-terminal and Edge-valued Decision Diagram LibrarY.
    Copyright (C) 2011, Iowa State University Research Foundation, Inc.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "meddly.h"
#include "meddly_expert.h"
#include "simple_model.h"
#include "timer.h"
#include "reorder.h"

// #define DUMP_NSF
// #define DUMP_REACHABLE

const char* kanban[] = {
  "X-+..............",  // Tin1
  "X.-+.............",  // Tr1
  "X.+-.............",  // Tb1
  "X.-.+............",  // Tg1
  "X.....-+.........",  // Tr2
  "X.....+-.........",  // Tb2
  "X.....-.+........",  // Tg2
  "X+..--+..-+......",  // Ts1_23
  "X.........-+.....",  // Tr3
  "X.........+-.....",  // Tb3
  "X.........-.+....",  // Tg3
  "X....+..-+..--+..",  // Ts23_4
  "X.............-+.",  // Tr4
  "X.............+-.",  // Tb4
  "X............+..-",  // Tout4
  "X.............-.+"   // Tg4
};

using namespace MEDDLY;

int usage(const char* who)
{
  /* Strip leading directory, if any: */
  const char* name = who;
  for (const char* ptr=who; *ptr; ptr++) {
    if ('/' == *ptr) name = ptr+1;
  }
  printf("\nUsage: %s nnnn (-bfs) (-dfs) (-exp)\n\n", name);
  printf("\tnnnn: number of parts\n");
  printf("\t-bfs: use traditional iterations\n");
  printf("\t-dfs: use saturation\n");
  printf("\t-exp: use explicit (very slow)\n\n");
  printf("\t--batch b: specify explicit batch size\n\n");
  return 1;
}

void printStats(const char* who, const forest* f)
{
  printf("%s stats:\n", who);
  const expert_forest* ef = (expert_forest*) f;
  ef->reportStats(stdout, "\t",
    expert_forest::HUMAN_READABLE_MEMORY  |
    expert_forest::BASIC_STATS | expert_forest::EXTRA_STATS |
    expert_forest::STORAGE_STATS | expert_forest::HOLE_MANAGER_STATS
  );
}

int main(int argc, const char** argv)
{
  int N = -1;
  char method = 'd';
  int batchsize = 256;

  for (int i=1; i<argc; i++) {
    if (strcmp("-bfs", argv[i])==0) {
      method = 'b';
      continue;
    }
    if (strcmp("-dfs", argv[i])==0) {
      method = 'd';
      continue;
    }
    if (strcmp("-exp", argv[i])==0) {
      method = 'e';
      continue;
    }
    if (strcmp("--batch", argv[i])==0) {
      i++;
      if (argv[i]) batchsize = atoi(argv[i]);
      continue;
    }
    N = atoi(argv[i]);
  }

  if (N<0) return usage(argv[0]);

  char reorderStrategy[10];
  printf("Enter the reordering strategy (LC, LI, BU, HI, BD):\n");
  scanf("%s", reorderStrategy);

  MEDDLY::initialize();

  printf("+-------------------------------------------+\n");
  printf("|   Initializing Kanban model for N = %-4d  |\n", N);
  printf("+-------------------------------------------+\n");
  fflush(stdout);

  // Initialize domain
  int* sizes = new int[16];
  for (int i=15; i>=0; i--) sizes[i] = N+1;
  domain* d = createDomainBottomUp(sizes, 16);
  delete[] sizes;

  // Set up MDD options
  forest::policies pmdd(false);
  if(strcmp("LC", reorderStrategy)==0) {
	  printf("Lowest Cost\n");
	  pmdd.setLowestCost();
  }
  else if(strcmp("LI", reorderStrategy)==0) {
	  printf("Lowest Inversion\n");
	  pmdd.setLowestInversion();
  }
  else if(strcmp("BU", reorderStrategy)==0) {
	  printf("Bubble Up\n");
	  pmdd.setBubbleUp();
  }
  else if(strcmp("HI", reorderStrategy)==0) {
	  printf("Highest Inversion\n");
	  pmdd.setHighestInversion();
  }
  else if(strcmp("BD", reorderStrategy)==0) {
	  printf("Bubble Down\n");
	  pmdd.setBubbleDown();
  }
  else {
	  exit(0);
  }

  // Build initial state
  int* initial = new int[17];
  for (int i=16; i; i--) initial[i] = 0;
  initial[1] = initial[5] = initial[9] = initial[13] = N;
  forest* mdd = d->createForest(0, forest::BOOLEAN, forest::MULTI_TERMINAL, pmdd);
  dd_edge init_state(mdd);
  mdd->createEdge(&initial, 1, init_state);
  delete[] initial;

  // Build next-state function
  forest* mxd = d->createForest(1, forest::BOOLEAN, forest::MULTI_TERMINAL);
  dd_edge nsf(mxd);
  if ('e' != method) {
    buildNextStateFunction(kanban, 16, mxd, nsf, 4);
#ifdef DUMP_NSF
    printf("Next-state function:\n");
    nsf.show(stdout, 2);
#endif
    printStats("MxD", mxd);
  }

  dd_edge reachable(mdd);
  switch (method) {
    case 'b':
        printf("Building reachability set using traditional algorithm\n");
        fflush(stdout);
        apply(REACHABLE_STATES_BFS, init_state, nsf, reachable);
        break;

    case 'd':
        printf("Building reachability set using saturation\n");
        fflush(stdout);
        apply(REACHABLE_STATES_DFS, init_state, nsf, reachable);
        break;

    case 'e': 
        printf("Building reachability set using explicit search\n");
        printf("Using batch size: %d\n", batchsize);
        fflush(stdout);
        explicitReachset(kanban, 16, mdd, init_state, reachable, batchsize);
        break;

    default:
        printf("Error - unknown method\n");
        exit(2);
  };
  printf("Done\n");
  fflush(stdout);

#ifdef DUMP_REACHABLE
  printf("Reachable states:\n");
  reachable.show(stdout, 2);
#endif

  printStats("MDD", mdd);
  fflush(stdout);

  double c;
  apply(CARDINALITY, reachable, c);
  operation::showAllComputeTables(stdout, 2);

  printf("Approx. %g reachable states\n", c);
  
  // Reorder
  int* newOrder = NULL;
  int* oldOrder = static_cast<int*>(calloc(16+1, sizeof(int)));
  for(int i=1; i<16+1; i++) {
	  oldOrder[i]=i;
  }

  char orderFile[200];
  printf("Please enter the order file:\n");
  scanf("%s", orderFile);

  readOrderFile(orderFile, newOrder, 16);
//  shuffle(newOrder, 1, 16);

  timer start;
  start.note_time();
  static_cast<expert_domain*>(d)->reorderVariables(newOrder);
  start.note_time();
  printf("Reorder Time: %f seconds\n", start.get_last_interval()/1000000.0);

  start.note_time();
  static_cast<expert_domain*>(d)->reorderVariables(oldOrder);
  start.note_time();
  printf("Reorder Time: %f seconds\n", start.get_last_interval()/1000000.0);

  delete[] newOrder;
  delete[] oldOrder;

  // cleanup
  MEDDLY::cleanup();
  return 0;
}

