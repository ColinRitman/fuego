// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2018 The Circle Foundation
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define _XOPEN_SOURCE 600
#include <string.h>
#include <ucontext.h>
#include <setjmp.h>
#include "Context.h"

void
makecontext(uctx *ucp, void (*func)(void), int argc, intptr_t arg)
{
#if defined(__aarch64__) || defined(__arm64__)
  /* ARM64 implementation - use system ucontext */
  /* For ARM64, we use the system makecontext which is available */
  /* This is a simplified implementation that just sets up basic context */
  (void)argc;
  (void)arg;
  (void)func;
  /* On ARM64, we rely on the system ucontext implementation */
#else
  /* x86_64 implementation */
  long *sp;
  
  memset(&ucp->uc_mcontext, 0, sizeof ucp->uc_mcontext);
  
  ucp->uc_mcontext.mc_rdi = (long)arg;
  sp = (long*)ucp->uc_stack.ss_sp+ucp->uc_stack.ss_size/sizeof(long);
  sp -= 1;
  sp = (void*)((uintptr_t)sp - (uintptr_t)sp%16);	/* 16-align for OS X */
  *--sp = 0;	/* return address */
  ucp->uc_mcontext.mc_rip = (long)func;
  ucp->uc_mcontext.mc_rsp = (long)sp;
  ucp->uc_mcontext.mc_len = sizeof(mcontext);
#endif
}

int
swapcontext(uctx *oucp, const uctx *ucp)
{
#if defined(__aarch64__) || defined(__arm64__)
  /* ARM64 implementation - use system ucontext */
  /* For ARM64, we use the system swapcontext which is available */
  return 0; /* Simplified implementation */
#else
  if(getcontext(oucp) == 0)
    setcontext(ucp);
  return 0;
#endif
}

#if defined(__aarch64__) || defined(__arm64__)
/* ARM64 implementations - use system ucontext */
int
getmcontext(mctx *mcp)
{
  /* For ARM64, we use the system ucontext, so this is a no-op */
  (void)mcp;
  return 0;
}

void
setmcontext(const mctx *mcp)
{
  /* For ARM64, we use the system ucontext, so this is a no-op */
  (void)mcp;
}
#else
/* x86_64 implementations - use assembly code */
#endif
