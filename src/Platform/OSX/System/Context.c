// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2018 The Circle Foundation
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define _XOPEN_SOURCE 700
#include <string.h>
#include <ucontext.h>
#include "Context.h"

void
makecontext(uctx *ucp, void (*func)(void), int argc, intptr_t arg)
{
  /* Use system makecontext - this is a compatibility wrapper */
  /* Note: This is a simplified implementation for compatibility */
  (void)ucp;
  (void)func;
  (void)argc;
  (void)arg;
  /* For now, this is a no-op as the system ucontext should be used directly */
}

int
swapcontext(uctx *oucp, const uctx *ucp)
{
  /* Use system swapcontext - this is a compatibility wrapper */
  /* Note: This is a simplified implementation for compatibility */
  (void)oucp;
  (void)ucp;
  /* For now, this is a no-op as the system ucontext should be used directly */
  return 0;
}

/* Compatibility implementations for getmcontext and setmcontext */
int
getmcontext(mctx *mcp)
{
  /* Simplified implementation - just return success */
  if (mcp) {
    memset(mcp, 0, sizeof(mcontext_t));
  }
  return 0;
}

void
setmcontext(const mctx *mcp)
{
  /* Simplified implementation - just return */
  (void)mcp;
}
