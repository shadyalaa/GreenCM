/*
 * File:
 *   mod_stats.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for gathering global statistics about transactions.
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "mod_stats.h"

#include "atomic.h"
#include "stm.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

volatile int number_of_threads = -1;
static mod_stats_data_t *stats_container[64];
static int running = 0;

static int mod_stats_key;
static int mod_stats_initialized = 0;

//mod_stats_global = { 0, 0, 0, ULONG_MAX, 0, 0, 0 };

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Return aggregate statistics about transactions.
 */
int stm_get_global_stats(const char *name, void *val)
{
  running = 0;
  if (!mod_stats_initialized) {
    fprintf(stderr, "Module mod_stats not initialized\n");
    exit(1);
  }

  if (strcmp("global_nb_commits", name) == 0) {
    *(unsigned long *)val = mod_stats_global.commits;
    return 1;
  }
  if (strcmp("global_nb_aborts", name) == 0) {
    *(unsigned long *)val = mod_stats_global.retries_acc;
    return 1;
  }
  if (strcmp("global_max_retries", name) == 0) {
    *(unsigned long *)val = mod_stats_global.retries_max;
    return 1;
  }

  return 0;
}

/*
 * Return statistics about current thread.
 */
int stm_get_local_stats(const char *name, void *val)
{
  mod_stats_data_t *stats;

  if (!mod_stats_initialized) {
    fprintf(stderr, "Module mod_stats not initialized\n");
    exit(1);
  }

  stats = (mod_stats_data_t *)stm_get_specific(mod_stats_key);
  assert(stats != NULL);

  if (strcmp("nb_commits", name) == 0) {
    *(unsigned long *)val = stats->commits;
    return 1;
  }
  if (strcmp("nb_aborts", name) == 0) {
    *(unsigned long *)val = stats->retries_acc;
    return 1;
  }
  if (strcmp("nb_retries_avg", name) == 0) {
    *(double *)val = (double)stats->retries_acc / stats->retries_cnt;
    return 1;
  }
  if (strcmp("nb_retries_min", name) == 0) {
    *(unsigned long *)val = stats->retries_min;
    return 1;
  }
  if (strcmp("nb_retries_max", name) == 0) {
    *(unsigned long *)val = stats->retries_max;
    return 1;
  }

  return 0;
}

/*
 * Called upon thread creation.
 */
static void mod_stats_on_thread_init(void *arg)
{
  //ATOMIC_FETCH_INC_FULL(&number_of_threads);

  mod_stats_data_t *stats;

  if ((stats = (mod_stats_data_t *)malloc(sizeof(mod_stats_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  stats->commits = 0;
  stats->retries = 0;
  stats->retries_acc = 0;
  stats->retries_cnt = 0;
  stats->retries_min = ULONG_MAX;
  stats->retries_max = 0;
stats->backoff_wait_0 = 0;
stats->backoff_wait_1 = 0;
stats->backoff_wait_2 = 0;
stats->backoff_wait_4 = 0;
stats->backoff_wait_5 = 0;
stats->backoff_wait_6 = 0;
stats->backoff_wait_7 = 0;
stats->backoff_wait_8 = 0;
stats->backoff_wait_9 = 0;
  stm_set_specific(mod_stats_key, stats);


  //ATOMIC_FETCH_INC_FULL(&number_of_threads);
  //ATOMIC_FETCH_ADD_FULL(&number_of_threads, 1);
  //printf("number of threads: %d\n",number_of_threads);
  //stats_container[number_of_threads] = stats;
}

/*
 * Called upon thread deletion.
 */
static void mod_stats_on_thread_exit(void *arg)
{
  mod_stats_data_t *stats;
  unsigned long max, min;

  stats = (mod_stats_data_t *)stm_get_specific(mod_stats_key);
  assert(stats != NULL);

  ATOMIC_FETCH_ADD_FULL(&mod_stats_global.commits, stats->commits);
  ATOMIC_FETCH_ADD_FULL(&mod_stats_global.retries_cnt, stats->retries_cnt);
  ATOMIC_FETCH_ADD_FULL(&mod_stats_global.retries_acc, stats->retries_acc);
retry_max:
  max = ATOMIC_LOAD(&mod_stats_global.retries_max);
  if (stats->retries_max > max) {
    if (ATOMIC_CAS_FULL(&mod_stats_global.retries_max, max, stats->retries_max) == 0)
      goto retry_max;
  }
retry_min:
  min = ATOMIC_LOAD(&mod_stats_global.retries_min);
  if (stats->retries_min < min) {
    if (ATOMIC_CAS_FULL(&mod_stats_global.retries_min, min, stats->retries_min) == 0)
      goto retry_min;
  }

  free(stats);
}

/*
 * Called upon transaction commit.
 */
static void mod_stats_on_commit(void *arg)
{
  mod_stats_data_t *stats;

  stats = (mod_stats_data_t *)stm_get_specific(mod_stats_key);
  assert(stats != NULL);
  stats->commits++;
  stats->retries_acc += stats->retries;
  stats->retries_cnt++;
  if (stats->retries_min > stats->retries)
    stats->retries_min = stats->retries;
  if (stats->retries_max < stats->retries)
    stats->retries_max = stats->retries;
  stats->retries = 0;
}

/*
 * Called upon transaction abort.
 */
static void mod_stats_on_abort(void *arg)
{
  mod_stats_data_t *stats;

  stats = (mod_stats_data_t *)stm_get_specific(mod_stats_key);
  assert(stats != NULL);

  stats->retries++;
}

/*
hill_climb_asym_threshold(void *x){
	unsigned long total_commits = 0;
	printf("number of threads: %d",number_of_threads);
	while(running){
		usleep(1000000);
		int i = 0;
		for (i=0;i<number_of_threads;i++){
			ATOMIC_FETCH_ADD_FULL(&total_commits, stats_container[i]->commits);
			//total_commits += &(stats_container[i])->commits;
		}
		printf("total commits: %d\n",total_commits);
	}
}*/

/*
 * Initialize module.
 */
void mod_stats_init(void)
{
  if (mod_stats_initialized)
    return;

  //mod_stats_global = { 0, 0, 0, ULONG_MAX, 0, 0, 0 };
  mod_stats_global.commits = 0;
  mod_stats_global.retries = 0;
  mod_stats_global.retries_min = 0;
  mod_stats_global.retries_max = 0;
  mod_stats_global.retries_acc = 0;
  mod_stats_global.retries_acc = 0;




  stm_register(mod_stats_on_thread_init, mod_stats_on_thread_exit, NULL, NULL, mod_stats_on_commit, mod_stats_on_abort, NULL);
  mod_stats_key = stm_create_specific();
  if (mod_stats_key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  mod_stats_initialized = 1;


 //running = 1;
 //pthread_t t1;
 //pthread_create(&t1, NULL, hill_climb_asym_threshold, NULL);


}