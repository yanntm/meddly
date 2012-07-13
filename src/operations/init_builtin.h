

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

namespace MEDDLY {
  class builtin_initializer;
};

class MEDDLY::builtin_initializer : public op_initializer {
  unary_opname* COPY;
  unary_opname* CARD;
  unary_opname* COMPL;
  unary_opname* MAXRANGE;
  unary_opname* MINRANGE;
  unary_opname* MDD2EVPLUS;

  binary_opname* UNION;
  binary_opname* INTERSECT;
  binary_opname* DIFFERENCE;
  binary_opname* CROSS;

  binary_opname* MIN;
  binary_opname* MAX;
  binary_opname* PLUS;
  binary_opname* MINUS;
  binary_opname* MULTIPLY;
  binary_opname* DIVIDE;

  binary_opname* EQ;
  binary_opname* NE;
  binary_opname* LT;
  binary_opname* LE;
  binary_opname* GT;
  binary_opname* GE;

  binary_opname* PRE_IMAGE;
  binary_opname* POST_IMAGE;
  binary_opname* FORWARD_DFS;
  binary_opname* FORWARD_BFS;
  binary_opname* BACKWARD_DFS;
  binary_opname* BACKWARD_BFS;

  numerical_opname* VECT_MATR_MULT;
  numerical_opname* MATR_VECT_MULT;

public:
  builtin_initializer(op_initializer* b) : op_initializer(b) { }
protected:
  virtual void init(const settings &s);
  virtual void cleanup();
};
