
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



/**
 * test_temporary_nodes.cc
 *
 * Testing operations on Temporary nodes in Meddly's Expert interface.
 */

#include <iostream>
#include <string.h>
#include "meddly.h"
#include "meddly_expert.h"

// Timer class
#include "timer.h"

#define VERBOSE

void printUsage(FILE *outputStream)
{
  fprintf(outputStream,
      "Usage: test_temporary_nodes <#Variables> <VariableBound> <#Elements>\n");
}


int reduceTemporaryNode(std::map<int, int>& cache, expert_forest* f, int root)
{
  assert(!f->isTerminalNode(root) && !f->isReducedNode(root));
  assert(f->isFullNode(root));

  // Check cache for result
  std::map<int, int>::iterator iter = cache.find(root);
  if (iter != cache.end()) {
    // Cache hit
    f->linkNode(iter->second);
    f->unlinkNode(root);
    return iter->second;
  }

  int temp = 0;
  int dptr = 0;
  int size = f->getFullNodeSize(root);

  for (int i = 0; i < size; ++i)
  {
    dptr = f->getFullNodeDownPtr(root, i);

    // Ignore terminal nodes and reduced nodes
    if (f->isTerminalNode(dptr) || f->isReducedNode(dptr)) continue;

    temp = reduceTemporaryNode(cache, f, dptr);
    // At this point, temp's incount has been increased and
    // dptr's incount has been decreased by 1.

    // Using WoUnlink, since dptr's has already been decreased
    f->setDownPtrWoUnlink(root, i, temp);
    // Unlinking temp because setDownPtr increases temp's incount by 1
    f->unlinkNode(temp);
  }

  temp = f->reduceNode(root);

  // Save result in cache
  if (f->isActiveNode(root)) cache[root] = temp;

  return temp;
}


int reduceTemporaryNode(expert_forest* f, int tempNode)
{
  // Recursive procedure:
  // Start at root (i.e. tempNode).
  // Build a temporary node for root.
  // -- Build reduced nodes for each child(root).
  // Keep track of duplicates via a compute cache which maps
  //   each temporary node to a reduced node.

  if (f->isTerminalNode(tempNode) || f->isReducedNode(tempNode)) {
    return tempNode;
  }

  std::map<int, int> cache;
  return reduceTemporaryNode(cache, f, tempNode);
}


int convertToTemporaryNode(std::map<int, int>& cache,
    expert_forest* f, int root)
{
  // Terminal nodes
  if (f->isTerminalNode(root)) return root;
  // Check cache for result
  std::map<int, int>::iterator iter = cache.find(root);
  if (iter != cache.end()) {
    f->linkNode(iter->second);
    return iter->second;
  }
  // Build new temporary node
  int level = f->getNodeLevel(root);
  int result = 0;
  if (f->isFullNode(root)) {
    int size = f->getFullNodeSize(root);
    result = f->createTempNode(level, size, false);
    for (int i = 0; i < size; i++)
    {
      int temp = convertToTemporaryNode(cache, f, f->getDownPtr(root, i));
      f->setDownPtrWoUnlink(result, i, temp);
      f->unlinkNode(temp);
    }
  }
  else {
    assert(f->isSparseNode(root));
    int sparseSize = f->getSparseNodeSize(root);
    int size = 1 + f->getSparseNodeIndex(root, sparseSize - 1);
    result = f->createTempNode(level, size, true);
    for (int i = 0; i < sparseSize; i++)
    {
      int temp = convertToTemporaryNode(cache, f,
          f->getSparseNodeDownPtr(root, i));
      f->setDownPtrWoUnlink(result, f->getSparseNodeIndex(root, i), temp);
      f->unlinkNode(temp);
    }
  }
  // Store result in cache
  cache[root] = result;
  return result;
}


void convertDDEdgeToTemporaryNode(const dd_edge& a, int& b)
{
  // Recursive procedure:
  // Start at root(a).
  // Build a temporary node for root(a).
  // -- Build a temporary node for each child(root(a)).
  // Keep track of duplicates via a compute cache which maps
  //   each reduced node to a temporary node.
  expert_forest* f = static_cast<expert_forest*>(a.getForest());
  int root = a.getNode();
  std::map<int, int> cache;
  b = convertToTemporaryNode(cache, f, root);
}


int main(int argc, char *argv[])
{
  if (argc != 4) {
    printUsage(stdout);
    exit(1);
  }

  srandom(1u);

  // initialize number of variables, their bounds and the number of elements
  // to create

  int nVariables = 0;
  int variableBound = 0;
  int nElements = 0;

  sscanf(argv[1], "%d", &nVariables);
  assert(nVariables > 0);

  sscanf(argv[2], "%d", &variableBound);
  assert(variableBound > 0);

  sscanf(argv[3], "%d", &nElements);
  assert(nElements > 0);

  printf("#variables: %d, variable bound: %d, #elements: %d\n",
      nVariables, variableBound, nElements);

  // create the elements randomly

  int** elements = (int **) malloc(nElements * sizeof(int *));
  for (int i = 0; i < nElements; ++i)
  {
    elements[i] = (int *) malloc((nVariables + 1) * sizeof(int));
    elements[i][0] = 0;
    for (int j = nVariables; j >= 1; --j)
    {
      elements[i][j] = int(float(variableBound) * random() / (RAND_MAX + 1.0));
      assert(elements[i][j] >= 0 && elements[i][j] < variableBound);
    }
    // print element[i]
#ifdef VERBOSE
    printf("Element %d: [%d", i, elements[i][0]);
    for (int j = 1; j <= nVariables; ++j)
    {
      printf(" %d", elements[i][j]);
    }
    printf("]\n");
#endif
  }

  // initialize the variable bounds array to provide to the domain

  int* bounds = (int *) malloc(nVariables * sizeof(int));
  assert(bounds != 0);
  for (int i = 0; i < nVariables; ++i)
  {
    bounds[i] = variableBound;
  }

  // Create a domain
  domain *d = MEDDLY_createDomain();
  assert(d != 0);
  assert(domain::SUCCESS == d->createVariablesBottomUp(bounds, nVariables));

  // Create an MDD forest in this domain (to store states)
  forest* states = d->createForest(false, forest::BOOLEAN,
      forest::MULTI_TERMINAL);
  assert(states != 0);

#if 0
  assert(forest::SUCCESS ==
      //states->setReductionRule(forest::FULLY_REDUCED));
    states->setReductionRule(forest::QUASI_REDUCED));
  assert(forest::SUCCESS ==
      states->setNodeDeletion(forest::OPTIMISTIC_DELETION));
  // states->setNodeDeletion(forest::PESSIMISTIC_DELETION));
  if (variableBound < 4) {
    assert(forest::SUCCESS ==
        states->setNodeStorage(forest::FULL_STORAGE));
  } else {
    assert(forest::SUCCESS ==
        states->setNodeStorage(forest::FULL_OR_SPARSE_STORAGE));
  }
#endif

  expert_forest* expertStates = dynamic_cast<expert_forest*>(states);
  assert(0 != expertStates);

  dd_edge initialState(states);

  // Use Meddly's batch addition to combine all elements in one step.
  assert(forest::SUCCESS ==
      states->createEdge(elements, nElements, initialState));

  int temporaryNode = 0;
  convertDDEdgeToTemporaryNode(initialState, temporaryNode);

  printf("Initial State Graph\n");
  printf("-------------------\n");
  expertStates->showNodeGraph(stdout, initialState.getNode());
  printf("Temporary Node Graph\n");
  printf("--------------------\n");
  expertStates->showNodeGraph(stdout, temporaryNode);

#if 0
  expertStates->unlinkNode(temporaryNode);
#else
  int reducedNode = reduceTemporaryNode(expertStates, temporaryNode);
  dd_edge final(states);
  final.set(reducedNode, 0, expertStates->getNodeLevel(reducedNode));
  printf("%s\n",
      (final == initialState)? "final == initial": "final != initial");
#endif

  // Cleanup; in this case simply delete the domain
  delete d;

  free(bounds);
  for (int i = 0; i < nElements; ++i)
  {
    free(elements[i]);
  }
  free(elements);

  return 0;
}
