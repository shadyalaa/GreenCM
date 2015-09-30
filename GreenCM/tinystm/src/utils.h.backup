/*
 * File:
 *   utils.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Utilities functions.
 *
 * Copyright (c) 2007-2012.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

#include <pthread.h>
#include <sched.h>

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>

#define COMPILE_TIME_ASSERT(pred)       switch (0) { case 0: case pred: ; }

#ifdef DEBUG2
# ifndef DEBUG
#  define DEBUG
# endif /* ! DEBUG */
#endif /* DEBUG2 */

#ifdef DEBUG
/* Note: stdio is thread-safe */
# define IO_FLUSH                       fflush(NULL)
# define PRINT_DEBUG(...)               printf(__VA_ARGS__); fflush(NULL)
#else /* ! DEBUG */
# define IO_FLUSH
# define PRINT_DEBUG(...)
#endif /* ! DEBUG */

#ifdef DEBUG2
# define PRINT_DEBUG2(...)              PRINT_DEBUG(__VA_ARGS__)
#else /* ! DEBUG2 */
# define PRINT_DEBUG2(...)
#endif /* ! DEBUG2 */

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#ifndef CACHELINE_SIZE
/* It ensures efficient usage of cache and avoids false sharing.
 * It could be defined in an architecture specific file. */
# define CACHELINE_SIZE                 64
#endif

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
# define likely(x)                      __builtin_expect(!!(x), 1)
# define unlikely(x)                    __builtin_expect(!!(x), 0)
# define INLINE                         inline __attribute__((always_inline))
# define NOINLINE                       __attribute__((noinline))
# if defined(__INTEL_COMPILER)
#  define ALIGNED                       /* Unknown */
# else /* ! __INTEL_COMPILER */
#  define ALIGNED                       __attribute__((aligned(CACHELINE_SIZE)))
# endif /* ! __INTEL_COMPILER */
#else /* ! (defined(__GNUC__) || defined(__INTEL_COMPILER)) */
# define likely(x)                      (x)
# define unlikely(x)                    (x)
# define INLINE                         inline
# define NOINLINE                       /* None in the C standard */
# define ALIGNED                        /* None in the C standard */
#endif /* ! (defined(__GNUC__) || defined(__INTEL_COMPILER)) */

static int stick_this_thread_to_core(int node);
static int stick_this_thread_to_node(int node);

/*
 * malloc/free wrappers for TM metadata.
 */
static INLINE void*
xmalloc(size_t size)
{
  void *memptr;
  /* TODO is posix_memalign is not available, provide malloc fallback. */
  /* Make sure that the allocation is aligned with cacheline size. */
#if defined(__CYGWIN__) || defined (__sun__)
  memptr = memalign(CACHELINE_SIZE, size);
#elif defined(__APPLE__)
  memptr = valloc(size);
#else
  if (posix_memalign(&memptr, CACHELINE_SIZE, size))
    return NULL;
#endif
  return memptr;
}

static INLINE void*
xrealloc(void *addr, size_t size)
{
  return realloc(addr, size);
}

static INLINE void
xfree(void *mem)
{
  free(mem);
}

static int stick_this_thread_to_core(int core){

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);
	return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

static int stick_this_thread_to_node(int node) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   int i;
        if (node == 0){
                for (i = 0; i < 16 ; i ++)
                        CPU_SET(i, &cpuset);
        }
        else if (node == 1){
                for (i = 16; i < 32 ; i ++)
                        CPU_SET(i, &cpuset);
        }
        else if (node == 2){
                for (i = 32; i < 48 ; i ++)
                        CPU_SET(i, &cpuset);
        }
        else if (node == 3){
                for (i = 48; i < 64 ; i ++)
                        CPU_SET(i, &cpuset);
        }
        else if (node == -1){
                for (i = 0; i < 64 ; i ++)
                        CPU_SET(i, &cpuset);
        }

   pthread_t current_thread = pthread_self();
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

#endif /* !_UTILS_H_ */

