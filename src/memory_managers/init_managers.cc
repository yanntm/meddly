
// $Id: init_builtin.cc 700 2016-07-07 21:06:50Z asminer $

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "../defines.h"
#include "init_managers.h"

#include "orig_grid.h"

namespace MEDDLY {
  const memory_manager_style* ORIGINAL_GRID = 0;
};




MEDDLY::memman_initializer::memman_initializer(initializer_list *p)
 : initializer_list(p)
{
  original_grid = 0;
}

void MEDDLY::memman_initializer::setup()
{
  memory_manager::resetGlobalStats();
  
  ORIGINAL_GRID = (original_grid  = new orig_grid_style);
}

void MEDDLY::memman_initializer::cleanup()
{
  delete original_grid;
  ORIGINAL_GRID = (original_grid  = 0);
}

