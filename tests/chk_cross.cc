
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

/*
    Tests the cross-product operator.
*/


#include <stdio.h>

#include "meddly.h"

using namespace MEDDLY;

// #define DEBUG_RANDSET

// #define DEBUG_SEGV

const int vars[] = {4, 4, 4, 4, 4, 4};
const int dontcare[] = {0, -1, -1, -1, -1, -1, -1};
int minterm[7];
const int* mtaddr[] = { minterm };
const int* dcaddr[] = { dontcare };

compute_manager* CM;

long seed = 123456789L;

double Random()
{
  const long MODULUS = 2147483647L;
  const long MULTIPLIER = 48271L;
  const long Q = MODULUS / MULTIPLIER;
  const long R = MODULUS % MULTIPLIER;

  long t = MULTIPLIER * (seed % Q) - R * (seed / Q);
  if (t > 0) {
    seed = t;
  } else {
    seed = t + MODULUS;
  }
  return ((double) seed / MODULUS);
}

int Equilikely(int a, int b)
{
  return (a + (int) ((b - a + 1) * Random()));
}

void randomizeMinterm()
{
  for (int i=1; i<7; i++) minterm[i] = Equilikely(-1, 3);
#ifdef DEBUG_RANDSET
  printf("Random minterm: [%d", minterm[1]);
  for (int i=2; i<7; i++) printf(", %d", minterm[i]);
  printf("]\n");
#endif
}

void makeRandomSet(forest* f, int nmt, dd_edge &x)
{
#ifdef DEBUG_SEGV
  fprintf(stderr, "\tentering makeRandomSet\n");
#endif
  dd_edge tmp(f);
  for (; nmt; nmt--) {
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tminterm\n");
#endif
    randomizeMinterm();
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tedge\n");
#endif
    f->createEdge(mtaddr, 1, tmp);
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tunion\n");
#endif
    x += tmp;
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tdone iter %d\n", nmt);
#endif
  }
#ifdef DEBUG_SEGV
  fprintf(stderr, "\texiting makeRandomSet\n");
#endif
}

void makeRandomRows(forest* f, int nmt, dd_edge &x)
{
#ifdef DEBUG_SEGV
  fprintf(stderr, "\tentering makeRandomRows\n");
#endif
  dd_edge tmp(f);
  for (; nmt; nmt--) {
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tminterm\n");
#endif
    randomizeMinterm();
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tedge\n");
#endif
    f->createEdge(mtaddr, dcaddr, 1, tmp);
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tunion\n");
#endif
    x += tmp;
#ifdef DEBUG_SEGV
    fprintf(stderr, "\tdone iter %d\n", nmt);
#endif
  }
#ifdef DEBUG_SEGV
  fprintf(stderr, "\texiting makeRandomRows\n");
#endif
}

void makeRandomCols(forest* f, int nmt, dd_edge &x)
{
  dd_edge tmp(f);
  for (; nmt; nmt--) {
    randomizeMinterm();
    f->createEdge(dcaddr, mtaddr, 1, tmp);
    x += tmp;
  }
}

void test(forest* mdd, forest* mxd, int nmt)
{
  dd_edge rs(mdd), cs(mdd);
  dd_edge one(mdd);
  mdd->createEdge(true, one);

  dd_edge rr(mxd), cr(mxd), rcr(mxd), tmp(mxd);

  long saveseed = seed;
  makeRandomSet(mdd, nmt, rs);
  seed = saveseed;
  makeRandomRows(mxd, nmt, rr);

#ifdef DEBUG_RANDSET
  printf("Generated random set:\n");
  rs.show(stdout, 2);
  printf("Generated random rows:\n");
  rr.show(stdout, 2);
#endif

  // check: generate rr from rs, make sure they match
  CM->apply(compute_manager::CROSS, rs, one, tmp);
#ifdef DEBUG_RANDSET
  printf("rs x 1:\n");
  tmp.show(stdout, 2);
#endif
  assert(tmp == rr);


  saveseed = seed;
  makeRandomSet(mdd, nmt, cs);
  seed = saveseed;
  makeRandomCols(mxd, nmt, cr);

#ifdef DEBUG_RANDSET
  printf("Generated random set:\n");
  cs.show(stdout, 2);
  printf("Generated random cols:\n");
  cr.show(stdout, 2);
#endif
  
  // check: generate cr from cs, make sure they match
  CM->apply(compute_manager::CROSS, one, cs, tmp);
#ifdef DEBUG_RANDSET
  printf("cs x 1:\n");
  tmp.show(stdout, 2);
#endif
  assert(tmp == cr);

  // intersection of rr and cr should equal rs x cs.
  CM->apply(compute_manager::CROSS, rs, cs, rcr);
  tmp = rr * cr;
  assert(tmp == rcr);
}


int main()
{
  initialize();
  CM = getComputeManager();
  assert(CM);

  domain* myd = createDomainBottomUp(vars, 6);
  assert(myd);

  forest* mdd = myd->createForest(0, forest::BOOLEAN, forest::MULTI_TERMINAL);
  assert(mdd);
  forest* mxd = myd->createForest(1, forest::BOOLEAN, forest::MULTI_TERMINAL);
  assert(mxd);

  for (int m=1; m<=20; m++) {
    printf("\tChecking cross-product for %2d random minterms\n", m);
    test(mdd, mxd, m);
  }
  destroyDomain(myd);
  cleanup();
  return 0;
}

