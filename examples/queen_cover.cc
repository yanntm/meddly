
// $Id:$

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
    Builds the set of solutions to the queen cover problem for 
    user-specified board size NxN.

    In other words, finds all possible ways to put queens onto
    an NxN chessboard so that every square either contains a queen,
    or is attached by a queen.
*/

#include <cstdio>

#include "../include/meddly.h"

int N;

compute_manager* CM;

dd_edge** qic;
dd_edge** qidp;
dd_edge** qidm;

void printmem(long m)
{
  if (m<1024) {
    printf("%d bytes", m);
    return;
  }
  double approx = m;
  approx /= 1024;
  if (approx < 1024) {
    printf("%3.2lf Kbytes", approx);
    return;
  }
  approx /= 1024;
  if (approx < 1024) {
    printf("%3.2lf Mbytes", approx);
    return;
  }
  approx /= 1024;
  if (approx < 1024) {
    printf("%3.2lf Gbytes", approx);
    return;
  }
  approx /= 1024;
  printf("%3.2lf Tbytes", approx);
}

forest* buildQueenForest()
{
  printf("Initializing domain and forest\n");
  int* vars = new int[N*N];
  for (int i=0; i<N*N; i++) {
    vars[i] = 2;
  }
  domain* d = MEDDLY_createDomain();
  assert(d);
  assert(domain::SUCCESS == d->createVariablesBottomUp(vars, N*N));
  forest* f = d->createForest(false, forest::INTEGER, forest::MULTI_TERMINAL);
  assert(f);

  // Set up MDD options
  assert(forest::SUCCESS == f->setReductionRule(forest::FULLY_REDUCED));
  assert(forest::SUCCESS == f->setNodeStorage(forest::FULL_OR_SPARSE_STORAGE));
  assert(forest::SUCCESS == f->setNodeDeletion(forest::PESSIMISTIC_DELETION));
  
  delete[] vars;
  return f;
}

inline int ijmap(int i, int j)
{
  return i*N+j+1;
}

void hasQueen(forest* f, int i, int j, dd_edge &e)
{
  assert(
    forest::SUCCESS == f->createEdgeForVar(ijmap(i, j), false, e)
  );
}

void queenInRow(forest* f, int r, dd_edge &e)
{
  assert(
    forest::SUCCESS == f->createEdge(int(0), e)
  );
  if (r<0 || r>=N) return;
  for (int j=0; j<N; j++) {
    dd_edge tmp(f);
    hasQueen(f, r, j, tmp);
    assert(
      compute_manager::SUCCESS == CM->apply(compute_manager::MAX, e, tmp, e)
    );
  }
}

void queenInCol(forest* f, int c, dd_edge &e)
{
  if (c>=0 && c<N && qic[c]) {
    e = *qic[c];
    return;
  }
  assert(
    forest::SUCCESS == f->createEdge(int(0), e)
  );
  if (c<0 || c>=N) return;
  for (int i=0; i<N; i++) {
    dd_edge tmp(f);
    hasQueen(f, i, c, tmp);
    assert(
      compute_manager::SUCCESS == CM->apply(compute_manager::MAX, e, tmp, e)
    );
  }
  qic[c] = new dd_edge(e);
}

void queenInDiagP(forest* f, int d, dd_edge &e)
{
  if (d>=0 && d<2*N-1 && qidp[d]) {
    e = *qidp[d];
    return;
  }
  assert(
    forest::SUCCESS == f->createEdge(int(0), e)
  );
  if (d<0 || d>=2*N-1) return;
  for (int i=0; i<N; i++) for (int j=0; j<N; j++) if (i+j==d) {
    dd_edge tmp(f);
    hasQueen(f, i, j, tmp);
    assert(
      compute_manager::SUCCESS == CM->apply(compute_manager::MAX, e, tmp, e)
    );
  }
  qidp[d] = new dd_edge(e);
}

void queenInDiagM(forest* f, int d, dd_edge &e)
{
  if (d>-N && d<N && qidm[d+N-1]) {
    e = *qidm[d+N-1];
    return;
  }
  assert(
    forest::SUCCESS == f->createEdge(int(0), e)
  );
  if (d<=-N || d>=N) return;
  for (int i=0; i<N; i++) for (int j=0; j<N; j++) if (i-j==d) {
    dd_edge tmp(f);
    hasQueen(f, i, j, tmp);
    assert(
      compute_manager::SUCCESS == CM->apply(compute_manager::MAX, e, tmp, e)
    );
  }
  qidm[d+N-1] = new dd_edge(e);
}

int main()
{
  CM = MEDDLY_getComputeManager();
  assert(CM);
  printf("Using %s\n", MEDDLY_getLibraryInfo(0));
  printf("Queen cover for NxN chessboard.  Enter the value for N:\n");
  scanf("%d", &N);
  if (N<1) return 0;

  forest* f = buildQueenForest();

  dd_edge num_queens(f);
  assert(
    forest::SUCCESS == f->createEdge(int(0), num_queens)
  );
  for (int i=0; i<N; i++) for (int j=0; j<N; j++) {
    dd_edge tmp(f);
    hasQueen(f, i, j, tmp);
    assert(compute_manager::SUCCESS ==
      CM->apply(compute_manager::PLUS, num_queens, tmp, num_queens)
    );
  } // for i,j

  qic = new dd_edge*[N];
  for (int i=0; i<N; i++) qic[i] = 0;
  qidp = new dd_edge*[2*N-1];
  qidm = new dd_edge*[2*N-1];
  for (int i=0; i<2*N-1; i++) {
    qidp[i] = qidm[i] = 0;
  }
  
  dd_edge** rowcov = new dd_edge*[N];
  
  for (int i=0; i<N; i++) {
    printf("Building constraint for row %2d\n", i+1);
    rowcov[i] = new dd_edge(f);
    assert(
      forest::SUCCESS == f->createEdge(int(1), *rowcov[i])
    );
    for (int j=0; j<N; j++) {
      dd_edge col(f);
      queenInCol(f, j, col);
      dd_edge dgp(f);
      queenInDiagP(f, i+j, dgp);
      dd_edge dgm(f);
      queenInDiagM(f, i-j, dgm);
      // "OR" these together
      assert(compute_manager::SUCCESS == 
          CM->apply(compute_manager::MAX, col, dgp, col)
      );
      assert(compute_manager::SUCCESS == 
          CM->apply(compute_manager::MAX, col, dgm, col)
      );
      // "AND" with this row
      assert(compute_manager::SUCCESS == 
          CM->apply(compute_manager::MULTIPLY, *rowcov[i], col, *rowcov[i])
      );
    } // for j
    // "OR" with "queen in this row"
    dd_edge qir(f);
    queenInRow(f, i, qir);
    assert(compute_manager::SUCCESS ==
        CM->apply(compute_manager::MAX, *rowcov[i], qir, *rowcov[i])
    );
  } // for i

  // Cleanup before we do the tough part
  for (int i=0; i<N; i++) delete qic[i];
  for (int i=0; i<2*N-1; i++) {
    delete qidp[i];
    delete qidm[i];
  }
  delete[] qic;
  delete[] qidp;
  delete[] qidm;

  dd_edge solutions(f);
  int q;
  for (q=1; q<N; q++) {
    printf("\nTrying to cover with %d queens\n", q);

    // Build all possible placements of q queens:
    assert(
      forest::SUCCESS == f->createEdge(q, solutions)
    );
    assert(compute_manager::SUCCESS ==
        CM->apply(compute_manager::EQUAL, solutions, num_queens, solutions)
    );

    // Now constrain to those that are covers
    fprintf(stderr, "\tCombining constraints\n\t\t");
    for (int i=0; i<N; i++) {
      fprintf(stderr, "%d ", N-i);
      assert(compute_manager::SUCCESS ==
        CM->apply(compute_manager::MULTIPLY, solutions, *rowcov[i], solutions)
      );
    } // for i
    fprintf(stderr, "\n");

    // Any solutions?
    if (0==solutions.getNode()) {
      printf("\tNo solutions\n");
      continue;
    }

    printf("\tSuccess\n");
    break;
  } // for q

  // Cleanup
  for (int i=0; i<N; i++) {
    delete rowcov[i];
  }
  delete[] rowcov;

  printf("Forest stats:\n");
  printf("\t%d current nodes\n", f->getCurrentNumNodes());
  printf("\t%d peak nodes\n", f->getPeakNumNodes());
  printf("\t");
  printmem(f->getCurrentMemoryUsed());
  printf(" current memory\n\t");
  printmem(f->getPeakMemoryUsed());
  printf(" peak memory\n");

  double c = solutions.getCardinality();
  printf("\nFor a %dx%d chessboard, ", N, N);
  printf("there are %lg covers with %d queens\n\n", c, q);

  // show one of the solutions
  dd_edge::const_iterator first = solutions.begin();
  dd_edge::const_iterator endIt = solutions.end();
  if (first != endIt) {
    const int* minterm = first.getAssignments();
    printf("One solution:\n\t");
    for (int i=0; i<N; i++) for (int j=0; j<N; j++) {
      if (minterm[ijmap(i,j)]) {
        printf("(%d, %d) ", i+1, j+1);
      }
    }
    printf("\n");
  }
  return 0;
}
