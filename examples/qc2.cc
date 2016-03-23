
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
    Builds the set of solutions to the queen cover problem for 
    user-specified board size NxN.

    In other words, finds all possible ways to put queens onto
    an NxN chessboard so that every square either contains a queen,
    or is attached by a queen.

    State:
      For each queen, where is it placed

    Constraints:
      conjunction over all squares:
        is this square covered
*/

#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "meddly.h"
#include "meddly_expert.h"
#include "loggers.h"

// #define SET_VAR_NAMES
// #define DEBUG_VARS
// #define DEBUG_ROW_CONSTRAINTS
// #define DEBUG_COLUMN_CONSTRAINTS
// #define DEBUG_PLUSD_CONSTRAINTS
// #define DEBUG_MINUSD_CONSTRAINTS
// #define DEBUG_SQUARES
// #define DEBUG_SPIRAL

// Force rows to be in order, to reduce number of solutions
#define ORDER_ROWS

// Force columns to be in order, if rows are equal
#define ORDER_COLS

using namespace MEDDLY;

FILE_output meddlyout(stdout);

/*
  Variable ordering.
  Give the variables for "queen i row" and "queen i column".
*/
class varorder {
  int* qr;
  int* qc;
  int M;
  const char* name;
public:
  varorder(int _M);
  ~varorder();

  // order is terminals, queen 1 row, queen 1 col, queen 2 row, queen 2 col, ...
  void byQueens();

  // order is terminals, queen 1 col, ..., queen M col, queen 1 row, ...
  void RowsCols();

  // order is terminals, queen 1 row, ..., queen M row, queen 1 col, ...
  void ColsRows();

  inline int queenRow(int i) const {
    return qr[i];
  }
  inline int queenCol(int i) const {
    return qc[i];
  }
  inline const char* Name() const {
    return name;
  }
  inline int queens() const {
    return M;
  }
};

varorder::varorder(int _M)
{
  M = _M;
  qr = new int[M];
  qc = new int[M];
}

varorder::~varorder()
{
  delete[] qr;
  delete[] qc;
}

void varorder::byQueens()
{
  int level = 0;
  for (int i=0; i<M; i++) {
    qc[i] = ++level;
    qr[i] = ++level;
  }
  name = "by queens";
}

void varorder::RowsCols()
{
  for (int i=0; i<M; i++) {
    qc[i] = i+1;
  }
  for (int i=0; i<M; i++) {
    qr[i] = M+i+1;
  }
  name = "rows above cols";
}

void varorder::ColsRows()
{
  for (int i=0; i<M; i++) {
    qr[i] = i+1;
  }
  for (int i=0; i<M; i++) {
    qc[i] = M+i+1;
  }
  name = "cols above rows";
}



forest::logger* buildLogger(const char* lfile, int lagg)
{
  if (0==lfile) return 0;
  static std::ofstream log;
  log.open(lfile, std::ofstream::out);
  if (!log) {
    printf("Couldn't open %s for writing, no logging\n", lfile);
    return 0;
  }
  if (lagg < 0) lagg = 16;
  printf("Opening log file %s, aggregating %d entries\n", lfile, lagg);
  forest::logger* LOG = new simple_logger(log, lagg);
  LOG->recordNodeCounts();
  LOG->addComment("Automatically generated by qc2 (queen cover)");
  return LOG;
}

forest* buildForest(forest::policies &p, int N, const varorder &V)
{
  int M = V.queens();

  /*
    Set up domain.
    2M variables (row and column of queen i),
      each with legal values 0..N-1.
  */
  printf("Initializing domain (%d variables)\n", 2*M);
  int* vars = new int[2*M];
  for (int i=0; i<2*M; i++) {
    vars[i] = N;
  }
  domain* d = createDomainBottomUp(vars, 2*M);
  delete[] vars;
  if (0==d) return 0;

#ifdef SET_VAR_NAMES
  char buffer[20];
  for (int i=0; i<M; i++) {
    snprintf(buffer, 20, "qr%02d", i+1);
    d->useVar(V.queenRow(i))->setName(strdup(buffer));
    snprintf(buffer, 20, "qc%02d", i+1);
    d->useVar(V.queenCol(i))->setName(strdup(buffer));
  }
#endif

  /*
    Set up forest.
    Only issue is to display the node deletion policy.
  */
  const char* ndp = "unknown node deletion";
  switch (p.deletion) {
    case forest::policies::NEVER_DELETE:
        ndp = "`never delete'";
        break;

    case forest::policies::OPTIMISTIC_DELETION:
        ndp = "optimistic node deletion";
        break;

    case forest::policies::PESSIMISTIC_DELETION:
        ndp = "pessimistic node deletion";
        break;
  }
  printf("Initializing forest with %s policy\n", ndp);
  return d->createForest(false, forest::INTEGER, forest::MULTI_TERMINAL, p);
}


/* 
  Build the function: 1 if queen i is in row r, 0 otherwise.
  We do this by hand because it is easy and fast.
*/
void queeniRowr(const varorder &V, int i, int r, dd_edge &e)
{
  expert_forest* F = dynamic_cast<expert_forest*>(e.getForest());
  
  node_builder& nb = F->useSparseBuilder(V.queenRow(i), 1);
  nb.i(0) = r;
  nb.d(0) = F->handleForValue(1);

  e.set( F->createReducedNode(-1, nb) );

#ifdef DEBUG_VARS
  printf("Build queen %d is in row %d:\n", i, r);
  e.show(meddlyout, 2);
#endif
}

/* 
  Build the function: 1 if queen i is in col c, 0 otherwise.
  We do this by hand because it is easy and fast.
*/
void queeniColc(const varorder &V, int i, int c, dd_edge &e)
{
  expert_forest* F = dynamic_cast<expert_forest*>(e.getForest());
  
  node_builder& nb = F->useSparseBuilder(V.queenCol(i), 1);
  nb.i(0) = c;
  nb.d(0) = F->handleForValue(1);

  e.set( F->createReducedNode(-1, nb) );

#ifdef DEBUG_VARS
  printf("Build queen %d is in col %d:\n", i, c);
  e.show(meddlyout, 2);
#endif
}

/*
  Build the function: 1 if queen i is on "plus diaginal" d, 0 otherwise.
  This is equivalent to: 1 if q[i].row + q[i].col == d, 0 otherwise.
*/
void queeniPlusd(const varorder &V, int i, int d, dd_edge &e)
{
  forest* F = e.getForest();

  dd_edge qr(F), qc(F), constd(F);

  F->createEdgeForVar(V.queenRow(i), false, qr);
  F->createEdgeForVar(V.queenCol(i), false, qc);
  F->createEdge(d, constd);

  apply(PLUS, qr, qc, e);
  apply(EQUAL, e, constd, e);
}

/*
  Build the function: 1 if queen i is on "minus diaginal" d, 0 otherwise.
  This is equivalent to: 1 if q[i].row - q[i].col == d, 0 otherwise.
*/
void queeniMinusd(const varorder &V, int i, int d, dd_edge &e)
{
  forest* F = e.getForest();

  dd_edge qr(F), qc(F), constd(F);

  F->createEdgeForVar(V.queenRow(i), false, qr);
  F->createEdgeForVar(V.queenCol(i), false, qc);
  F->createEdge(d, constd);

  apply(MINUS, qr, qc, e);
  apply(EQUAL, e, constd, e);
}

/*
  Build the function: 1 if there is some queen in row r, 0 otherwise.
*/
void queenInRow(const varorder &V, int r, dd_edge &e)
{
  forest* F = e.getForest();
  dd_edge qir(F);
  queeniRowr(V, 0, r, e);
  for (int i=1; i<V.queens(); i++) {
    queeniRowr(V, i, r, qir);
    apply(MAXIMUM, e, qir, e); 
  }
}

/*
  Build the function: 1 if there is some queen in col c, 0 otherwise.
*/
void queenInCol(const varorder &V, int c, dd_edge &e)
{
  forest* F = e.getForest();
  dd_edge qic(F);
  queeniColc(V, 0, c, e);
  for (int i=1; i<V.queens(); i++) {
    queeniColc(V, i, c, qic);
    apply(MAXIMUM, e, qic, e); 
  }
}

/*
  Build the function: 1 if there is some queen in plus diagonal d, 0 otherwise
*/
void queenInPlusd(const varorder &V, int d, dd_edge &e)
{
  forest* F = e.getForest();
  dd_edge qid(F);
  queeniPlusd(V, 0, d, e);
  for (int i=1; i<V.queens(); i++) {
    queeniPlusd(V, i, d, qid);
    apply(MAXIMUM, e, qid, e);
  }
}

/*
  Build the function: 1 if there is some queen in minus diagonal d, 0 otherwise
*/
void queenInMinusd(const varorder &V, int d, dd_edge &e)
{
  forest* F = e.getForest();
  dd_edge qid(F);
  queeniMinusd(V, 0, d, e);
  for (int i=1; i<V.queens(); i++) {
    queeniMinusd(V, i, d, qid);
    apply(MAXIMUM, e, qid, e);
  }
}


/*
  For each square in the board, build the function:
    1 if the square is covered by some queen, 0 otherwise.
*/
dd_edge** buildConstraintsForSquares(forest *F, const varorder &V, int N)
{
  dd_edge** covered = new dd_edge* [N];
  for (int i=0; i<N; i++) {
    dd_edge tmp(F);
    tmp.set(0);
    covered[i] = new dd_edge[N];
    for (int j=0; j<N; j++) {
      covered[i][j] = tmp;
    }
  }

  /*
    Add row coverage
  */
  for (int r=0; r<N; r++) {
    dd_edge tmp(F);
    queenInRow(V, r, tmp);
#ifdef DEBUG_ROW_CONSTRAINTS
    printf("queenInRow(%d):\n", r);
    tmp.show(meddlyout, 2);
#endif

    for (int c=0; c<N; c++) {
      apply(MAXIMUM, covered[r][c], tmp, covered[r][c]);
    }
  }

  /*
    Add column coverage
  */
  for (int c=0; c<N; c++) {
    dd_edge tmp(F);
    queenInCol(V, c, tmp);
#ifdef DEBUG_COLUMN_CONSTRAINTS
    printf("queenInCol(%d):\n", c);
    tmp.show(meddlyout, 2);
#endif

    for (int r=0; r<N; r++) {
      apply(MAXIMUM, covered[r][c], tmp, covered[r][c]);
    }
  }

  /*
    Add plus diagonal coverage
  */
  for (int d=0; d<2*N-1; d++) {
    dd_edge tmp(F);
    queenInPlusd(V, d, tmp);
#ifdef DEBUG_PLUSD_CONSTRAINTS
    printf("queenInPlusd(%d):\n", d);
    tmp.show(meddlyout, 2);
#endif

    // There is a fancier way to set up this loop...
    for (int r=0; r<N; r++) {
      // restrict to r+c=d
      int c = d-r;
      if (c<0) continue;
      if (c>=N) continue;

      apply(MAXIMUM, covered[r][c], tmp, covered[r][c]);
    }
  } 

  /*
    Add minus diagonal coverage
  */
  for (int d=1-N; d<N; d++) {
    dd_edge tmp(F);
    queenInMinusd(V, d, tmp);
#ifdef DEBUG_MINUSD_CONSTRAINTS
    printf("queenInMinusd(%d):\n", d);
    tmp.show(meddlyout, 2);
#endif

    // There is a fancier way to set up this loop...
    for (int r=0; r<N; r++) {
      // restrict to r-c=d
      int c = r-d;
      if (c>=N) continue;
      if (c<0) continue;

      apply(MAXIMUM, covered[r][c], tmp, covered[r][c]);
    }
  } 

#ifdef ORDER_ROWS
  /*
    Build row ordering constraint
  */
  dd_edge roworder(F);
  F->createEdge(1, roworder);
  for (int i=V.queens()-1; i; i--) {
    dd_edge rowi(F), rowim1(F), tmp(F);
    F->createEdgeForVar(V.queenRow(i), false, rowi);
    F->createEdgeForVar(V.queenRow(i-1), false, rowim1);
    apply(GREATER_THAN_EQUAL, rowi, rowim1, tmp);
    apply(MULTIPLY, roworder, tmp, roworder);
  }
  /*
    Intersect with all square constraints
  */
  for (int r=0; r<N; r++) {
    for (int c=0; c<N; c++) {
      apply(MULTIPLY, covered[r][c], roworder, covered[r][c]);
    }
  }
#endif

#ifdef ORDER_COLS
  /*
    Build column ordering constraint
  */
  dd_edge colorder(F);
  F->createEdge(1, colorder);
  for (int i=V.queens()-1; i; i--) {
    dd_edge rowi(F), rowim1(F), coli(F), colim1(F), rowsequal(F), tmp(F);
    F->createEdgeForVar(V.queenRow(i), false, rowi);
    F->createEdgeForVar(V.queenRow(i-1), false, rowim1);
    apply(EQUAL, rowi, rowim1, rowsequal);
    // rowsequal: if the rows are equal
    F->createEdgeForVar(V.queenCol(i), false, coli);
    F->createEdgeForVar(V.queenCol(i-1), false, colim1);
    apply(GREATER_THAN_EQUAL, coli, colim1, tmp);
    // tmp: order the columns...
    apply(MULTIPLY, tmp, rowsequal, tmp);
    // ...if the rows are equal
    dd_edge one(F);
    F->createEdge(1, one);
    apply(MINUS, one, rowsequal, rowsequal);
    // rowsequal: negated
    apply(MAXIMUM, rowsequal, tmp, tmp);
    //
    // Now, tmp is our rule:
    //  if rows are equal, then force column ordering;
    //  otherwise, if rows are not equal, then do not.
   
    // And it to the rest
    apply(MULTIPLY, colorder, tmp, colorder);
  }
  /*
    Intersect with all square constraints
  */
  for (int r=0; r<N; r++) {
    for (int c=0; c<N; c++) {
      apply(MULTIPLY, covered[r][c], colorder, covered[r][c]);
    }
  }
#endif

#ifdef DEBUG_SQUARES
  printf("\nConditions for each square:\n");
  for (int r=0; r<N; r++) {
    for (int c=0; c<N; c++) {
      printf("----------------------------------------\n");
      printf("[%2d, %2d]\n", r, c);
      printf("----------------------------------------\n");
      covered[r][c].show(meddlyout, 2);
    }
  }
#endif

  return covered;
}


/*
  "And" an array of constraints.
*/
void AndList(dd_edge *A, int n, bool dots)
{
  printf("Accumulating constraints\n");
  for (int i=n-2; i>=0; i--) {
    apply(MULTIPLY, A[i], A[i+1], A[i]);
    A[i+1].getForest()->createEdge(1, A[i+1]);
    if (dots) {
      if (A[i].getNode()) putc(',', stdout);
      else                putc('0', stdout);
      fflush(stdout);
    }
  }
  if (dots) putc('\n', stdout);
}

/*
  "And" sublists, then accumulate
*/
void AndSublists(dd_edge *A, int N, int n, bool dots)
{
  printf("Accumulating groups of %d constraints\n", n);

  /* First pass - collect groups of n */
  int res = 0;
  int i = 0;
  while (i<N) {
    dd_edge result(A[0].getForest());
    int stop = i+n;
    if (stop > N) stop = N;
    result = A[i];
    A[i].getForest()->createEdge(1, A[i]);
    for (i++; i<stop; i++) {
      apply(MULTIPLY, result, A[i], result);
      A[i].getForest()->createEdge(1, A[i]);
      if (dots) {
        if (result.getNode()) putc(',', stdout);
        else                  putc('0', stdout);
        fflush(stdout);
      }
    }
    A[res] = result;
    res++;
    if (dots) putc('\n', stdout);
  }

  /* Second pass - accumulate the groups */
  AndList(A, res, dots);
}

/*
  "And" an array of constraints, by successive folding.
*/
void FoldList(dd_edge *A, int n, bool dots)
{
  printf("Folding constraints\n");
  while (n>1) {
    dd_edge result(A[0].getForest());
    int res = 0;
    
    for (int i=0; i<n; i+=2) {

      if (i+1>=n) {
        result = A[i];
      } else {
        apply(MULTIPLY, A[i], A[i+1], result);
        A[i+1].getForest()->createEdge(1, A[i+1]);
        if (dots) {
          if (result.getNode()) putc(',', stdout);
          else                  putc('0', stdout);
          fflush(stdout);
        }
      }
      A[i].getForest()->createEdge(1, A[i]);
      A[res] = result;
      res++;
    }
    if (dots) {
      fputc('\n', stdout);
    }
    n = res;
  }
}

/*
  Board squares to list, by rows
*/
void FlattenByRows(dd_edge** squares, dd_edge* list, int N)
{
  printf("Ordering constraints by rows\n");
  int i = 0;
  for (int r=0; r<N; r++) {
    for (int c=0; c<N; c++) {
      list[i] = squares[r][c];
      i++;
    }
  }
}

/*
  Board squares to list, by cols
*/
void FlattenByCols(dd_edge** squares, dd_edge* list, int N)
{
  printf("Ordering constraints by cols\n");
  int i = 0;
  for (int c=0; c<N; c++) {
    for (int r=0; r<N; r++) {
      list[i] = squares[r][c];
      i++;
    }
  }
}

/*
  Board squares to list, by diagonals
*/
void FlattenByPlusDiags(dd_edge** squares, dd_edge* list, int N)
{
  printf("Ordering constraints by diagonals\n");
  int i = 0;

  for (int d=0; d<=2*N; d++) {
    // There is a fancier way to set up this loop...
    for (int r=0; r<N; r++) {
      // restrict to r+c=d
      int c = d-r;
      if (c<0) break; // out of bounds, and we will continue to be
      if (c>=N) continue;

      list[i] = squares[r][c];
      i++;
    }
  } 
}

/*
  Board squares to list, by inward spiral
*/
void FlattenByInwardSpiral(dd_edge** squares, dd_edge* list, int N)
{
  printf("Ordering constraints by inward spiral\n");
  int i = 0;

  int loCol = 0;
  int hiCol = N-1;
  int loRow = 0;
  int hiRow = N-1;
  int r = 0;
  int c = 0;

  for (;;) {

    /* Go to the right */
    for (c=loCol; c<=hiCol; c++) {
#ifdef DEBUG_SPIRAL
      printf("[%d,%d]\n", loRow, c);
#endif
      list[i] = squares[loRow][c];
      i++;
    }
    loRow++;
    if (loRow > hiRow) return; 

    /* Go down */
    for (r=loRow; r<=hiRow; r++) {
#ifdef DEBUG_SPIRAL
      printf("[%d,%d]\n", r, hiCol);
#endif
      list[i] = squares[r][hiCol];
      i++;
    }
    hiCol--;
    if (loCol > hiCol) return;

    /* Go left */
    for (c=hiCol; c>=loCol; c--) {
#ifdef DEBUG_SPIRAL
      printf("[%d,%d]\n", hiRow, c);
#endif
      list[i] = squares[hiRow][c];
      i++;
    }
    hiRow--;
    if (loRow > hiRow) return;

    /* Go up */
    for (r=hiRow; r>=loRow; r--) {
#ifdef DEBUG_SPIRAL
      printf("[%d,%d]\n", r, loCol);
#endif
      list[i] = squares[r][loCol];
      i++;
    }
    loCol++;
    if (loCol > hiCol) return;
  }
}

void matchQueens(varorder &V, int q1, int q2, dd_edge &rule)
{
  forest* F = rule.getForest();

  dd_edge q1r(F), q1c(F), q2r(F), q2c(F), tmp(F);

  F->createEdgeForVar(V.queenRow(q1), false, q1r);
  F->createEdgeForVar(V.queenRow(q2), false, q2r);

  // q1r == q2r
  apply(EQUAL, q1r, q2r, tmp);

  F->createEdgeForVar(V.queenCol(q1), false, q1c);
  F->createEdgeForVar(V.queenCol(q2), false, q2c);

  // q1c == q2c
  apply(EQUAL, q1c, q2c, rule);

  apply(MULTIPLY, rule, tmp, rule);
}

int usage(const char* who)
{
  /* Strip leading directory, if any: */
  const char* name = who;
  for (const char* ptr=who; *ptr; ptr++) {
    if ('/' == *ptr) name = ptr+1;
  }
  printf("Usage: %s [options] <outfile>\n\n", name);
  printf("Legal options:\n");
  printf("         -a o:  Accumulation order o.  Orders:\n");
  printf("                    c - by columns\n");
  printf("                    d - by diagonals\n");
  printf("                    r - by rows (default)\n");
  printf("                    s - spiral inward\n");
  printf("         -b s:  Batch accumulation style s.  Styles:\n");
  printf("                    f - fold.  For each pass, combine adjacent.\n");
  printf("                    h - `half square root'.  Collect by N/2.\n");
  printf("                    j - just accumulate, in order\n");
  printf("                    s - `square root'.  Collect by N. (default)\n");
  printf("     -l lfile:  Write logging information to specified file\n");
  printf("     -L count:  Aggregate count items per log entry\n");
  printf("         -m M:  specify maximum number of queens (default is N)\n");
  printf("         -n N:  specify board dimension as NxN\n");
  printf("           -o:  Optimistic node deletion\n");
  printf("           -p:  Pessimistic node deletion (default)\n");
  printf("         -v c:  Set the variable order to code `c'.  Codes:\n");
  printf("                    c - columns above rows rows\n");
  printf("                    q - by queens (default)\n");
  printf("                    r - rows above cols\n");
  printf("    <outfile>:  if specified, we write all solutions to this file\n\n");
  return 1;
}


int main(int argc, const char** argv)
{
  /* Parse command line */
  forest::policies p(false);
  p.setPessimistic();
  const char* lfile = 0;
  int lagg = 16;
  const char* ofile = 0;
  int N = -1;
  int M = -1;
  char vorder = 'q';
  char accorder = 'r';
  char accstyle = 's';

  for (int i=1; i<argc; i++) {
    if (argv[i][0]=='-') {
      if (argv[i][1]==0) return usage(argv[0]);
      if (argv[i][2]!=0) return usage(argv[0]);
      switch (argv[i][1]) {

        case 'l': i++;
                  lfile = argv[i];
                  continue;

        case 'L': i++;
                  if (argv[i]) lagg = atoi(argv[i]);
                  continue;

        case 'm': i++;
                  if (argv[i]) M = atoi(argv[i]);
                  continue;

        case 'n': i++;
                  if (argv[i]) N = atoi(argv[i]);
                  continue;
                  
        case 'o': p.setOptimistic();
                  continue;

        case 'p': p.setPessimistic();
                  continue;

        case 'v':
                  i++;
                  if (argv[i]) vorder = argv[i][0];
                  continue;

        case 'a':
                  i++;
                  if (argv[i]) accorder = argv[i][0];
                  continue;

        case 'b':
                  i++;
                  if (argv[i]) accstyle = argv[i][0];
                  continue;

        default:  return usage(argv[0]);
      } // switch

      // sanity check; should never get here
      continue;
    } // first char is '-'

    // this must be the output file name
    if (ofile) return usage(argv[0]);
    ofile = argv[i];
  }

  /*
    Sanitize N and M parameters
  */
  if (N<0) return usage(argv[0]);
  if (0==N) return 0;
  if (M<0) M=N;

  /*
    Set up MEDDLY
  */
  initialize();
  printf("Using %s\n", getLibraryInfo(0));
  printf("Queen cover, %dx%d board, trying at most %d queens\n", N, N, M);

  /*
    Set variable order
  */
  varorder V(M);
  switch (vorder) {
    case 'c': V.ColsRows();
              break;

    case 'r': V.RowsCols();
              break;

    case 'q': V.byQueens();
              break;

    default:  printf("Unknown variable order code `%c.'\n", vorder);
              return usage(argv[0]);
  }
  printf("Using variable order: %s\n", V.Name());
    
  /*
    Set up forest logging, if desired
  */
  forest::logger* LOG = buildLogger(lfile, lagg);

  /*
    Build forest
  */
  forest* F = buildForest(p, N, V);
  if (0==F) return 1;
  F->setLogger(LOG, "qc2 forest");
  if (LOG) LOG->newPhase("Building per square constraints");

  /*
    Build constraints for each square.
    Then give a quick report.
  */
  printf("Building covering conditions for each square\n");
  dd_edge** covered = buildConstraintsForSquares(F, V, N);
  printf("Basic constraints are done:\n");
  expert_forest* ef = (expert_forest*) F;
  ef->reportStats(meddlyout, "\t", 
    expert_forest::HUMAN_READABLE_MEMORY | expert_forest::BASIC_STATS
  );

  dd_edge* acc = new dd_edge[N*N];

  /*
    Build the list of constraints to combine.
    Only difference below is the order of the constraints.
  */
  switch (accorder) {
    case 'c':
                FlattenByCols(covered, acc, N);
                break;
    case 'd':   
                FlattenByPlusDiags(covered, acc, N);
                break;
    case 'r':
                FlattenByRows(covered, acc, N);
                break;
    case 's':
                FlattenByInwardSpiral(covered, acc, N);
                break;

    default :
                printf("Unknown accumulation order `%c', using `r' instead\n",
                  accorder);
                FlattenByRows(covered, acc, N);
                break;
  }

  for (int i=0; i<N; i++) {
    delete[] covered[i];
  }
  delete[] covered;

  if (LOG) LOG->newPhase("Accumulating constraints");

  /*
    Combine constraints, based on chosen style.
  */
  switch (accstyle) {
    case 'f':
              FoldList(acc, N*N, true);
              break;

    case 'j':
              AndList(acc, N*N, true);
              break;

    case 'h':
              AndSublists(acc, N*N, N/2, true);
              break;

    case 's':
    default:
              AndSublists(acc, N*N, N, true);
              break;


  }
  printf("Done!\n");
  
  int Q;  // minimum number of required queens

  if (0==acc[0].getNode()) {
    Q = 0;
    printf("\nNO SOLUTIONS\n\n");
  } else {
    printf("There are solutions.  Minimizing number of queens.\n");
    if (LOG) LOG->newPhase("Minimizing");

    Q = M-1;

    /*
      Constrain queen Q and Q-1 to be at the same position;
      if we still have a solution, then we can eliminate the
      last queen and continue.
    */
    for (; Q; Q--) {
      dd_edge killq(F);
      matchQueens(V, Q, Q-1, killq);
      apply(MULTIPLY, killq, acc[0], killq);

      if (0==killq.getNode()) break;

      printf("Queen %d is not needed\n", Q+1);
      acc[0] = killq;
    }

    printf("\n%d QUEENS MINIMAL SOLUTION\n\n", Q+1);
  
    long c;
    apply(CARDINALITY, acc[0], c);
    printf("For a %dx%d chessboard, ", N, N);
    printf("there are %ld covers with %d queens\n", c, Q+1);
  }

  printf("Forest stats:\n");
  ef->reportStats(meddlyout, "\t", 
    expert_forest::HUMAN_READABLE_MEMORY  |
    expert_forest::BASIC_STATS | expert_forest::EXTRA_STATS |
    expert_forest::STORAGE_STATS | expert_forest::HOLE_MANAGER_STATS
  );
  operation::showAllComputeTables(meddlyout, 3);

  /*
    Write solutions to file
  */
  if (ofile) {
    FILE* OUT = fopen(ofile, "w");
    if (0==OUT) {
      printf("Couldn't open %s for writing, no solutions will be written\n", ofile);      
    } else {
      fprintf(OUT, "%d # Board dimension\n\n", N);
      enumerator iter(acc[0]);
      for (long counter=1; iter; ++iter, ++counter) {
        fprintf(OUT, "solution %5ld:  ", counter);
        const int* minterm = iter.getAssignments();
        for (int q=0; q<=Q; q++) {
          int r = minterm[V.queenRow(q)];
          int c = minterm[V.queenCol(q)];
          fprintf(OUT, "(%2d, %2d) ", r+1, c+1);
        }
        fprintf(OUT, "\n");
      } // for iter
      fprintf(OUT, "\n");
      fclose(OUT);
    }
  }

  if (LOG) LOG->newPhase("Cleanup");
  cleanup();
  delete LOG;
  return 0;
}