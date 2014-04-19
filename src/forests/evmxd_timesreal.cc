
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

#include "evmxd_timesreal.h"


MEDDLY::evmxd_timesreal::evmxd_timesreal(int dsl, domain *d, const policies &p)
 : evmxd_forest(dsl, d, REAL, EVTIMES, p)
{
  setEdgeSize(sizeof(float), true);
  initializeForest();
}

MEDDLY::evmxd_timesreal::~evmxd_timesreal()
{ }

void MEDDLY::evmxd_timesreal::createEdge(float val, dd_edge &e)
{
  createEdgeTempl<OP, float>(val, e);
}

void MEDDLY::evmxd_timesreal
::createEdge(const int* const* vlist, const int* const* vplist,
  const float* terms, int N, dd_edge &e)
{
  binary_operation* unionOp = getOperation(PLUS, this, this, this);
  assert(unionOp);
  enlargeStatics(N);
  enlargeVariables(vlist, N, false);
  enlargeVariables(vplist, N, true);

  evmxd_edgemaker<OP, float>
  EM(this, vlist, vplist, terms, order, N,
    getDomain()->getNumVariables(), unionOp);

  float ev;
  node_handle ep;
  EM.createEdge(ev, ep);
  e.set(ep, ev);
}

void MEDDLY::evmxd_timesreal
::createEdgeForVar(int vh, bool vp, const float* terms, dd_edge& a)
{
  createEdgeForVarTempl<OP, float>(vh, vp, terms, a);
}

void MEDDLY::evmxd_timesreal
::evaluate(const dd_edge &f, const int* vlist, const int* vplist,
  float &term) const
{
  evaluateT<OP, float>(f, vlist, vplist, term);
}

bool MEDDLY::evmxd_timesreal
::areEdgeValuesEqual(const void* eva, const void* evb) const
{
  float val1, val2;
  OP::readValue(eva, val1);
  OP::readValue(evb, val2);
  return !OP::notClose(val1, val2);
}

bool MEDDLY::evmxd_timesreal::isRedundant(const node_builder &nb) const
{
  return isRedundantTempl<OP>(nb);
}

bool MEDDLY::evmxd_timesreal::isIdentityEdge(const node_builder &nb, int i) const
{
  return isIdentityEdgeTempl<OP>(nb, i); 
}

void MEDDLY::evmxd_timesreal::normalize(node_builder &nb, float& ev) const
{
  int minindex = -1;
  int stop = nb.isSparse() ? nb.getNNZs() : nb.getSize();
  for (int i=0; i<stop; i++) {
    if (0==nb.d(i)) continue;
    if (minindex < 0) { minindex = i; continue; }
    if (nb.ef(i) < nb.ef(minindex)) { minindex = i; }
  }
  if (minindex < 0) return; // this node will eventually be reduced to "0".
  ev = nb.ef(minindex);
  MEDDLY_DCASSERT(ev > 0.0f);
  for (int i=0; i<stop; i++) {
    if (0==nb.d(i)) continue;
    float temp;
    nb.getEdge(i, temp);
    temp /= ev;
    nb.setEdge(i, temp);
  }
}

void MEDDLY::evmxd_timesreal::showEdgeValue(FILE* s, const void* edge) const
{
  OP::show(s, edge);
}

void MEDDLY::evmxd_timesreal::writeEdgeValue(FILE* s, const void* edge) const
{
  OP::write(s, edge);
}

void MEDDLY::evmxd_timesreal::readEdgeValue(FILE* s, void* edge)
{
  OP::read(s, edge);
}

const char* MEDDLY::evmxd_timesreal::codeChars() const
{
  return "dd_etxr";
}
