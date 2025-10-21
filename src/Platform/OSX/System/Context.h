// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>

/* Use system ucontext on all platforms */
typedef ucontext_t uctx;
typedef mcontext_t mctx;

extern	int		fuego_swapcontext(uctx*, const uctx*);
extern	void		fuego_makecontext(uctx*, void(*)(void), int, intptr_t);
extern	int		fuego_getmcontext(mctx*);
extern	void		fuego_setmcontext(const mctx*);

/* Define macros to use our functions */
#define swapcontext fuego_swapcontext
#define makecontext fuego_makecontext
#define getmcontext fuego_getmcontext
#define setmcontext fuego_setmcontext

/* Use system mcontext_t and ucontext_t definitions */
  
#ifdef __cplusplus
}
#endif
