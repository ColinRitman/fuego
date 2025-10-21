// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2018 The Circle Foundation
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define _XOPEN_SOURCE 700
#include <string.h>
#include <ucontext.h>
#include <setjmp.h>
#include "Context.h"

void
fuego_makecontext(uctx *ucp, void (*func)(void), int argc, intptr_t arg)
{
  /* Use system makecontext */
  makecontext(ucp, func, argc, arg);
}

int
fuego_swapcontext(uctx *oucp, const uctx *ucp)
{
  /* Use system swapcontext */
  return swapcontext(oucp, ucp);
}

int
fuego_getmcontext(mctx *mcp)
{
  /* Use system getcontext */
  ucontext_t ucp;
  if (getcontext(&ucp) == 0) {
    *mcp = ucp.uc_mcontext;
    return 0;
  }
  return -1;
}

void
fuego_setmcontext(const mctx *mcp)
{
  /* Use system setcontext */
  ucontext_t ucp;
  ucp.uc_mcontext = *mcp;
  setcontext(&ucp);
}
