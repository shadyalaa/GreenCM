/*
 * File:
 *   stm.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   STM functions.
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

#define _GNU_SOURCE
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include "stm.h"
//#include "stm_internal.h"
#include "aperf.h"
#include "utils.h"
#include "atomic.h"
#include "gc.h"

#include <sched.h>


/* ################################################################### *
 * DEFINES
 * ################################################################### */

/* Indexes are defined in stm_internal.h  */
static const char *design_names[] = {
  /* 0 */ "WRITE-BACK (ETL)",
  /* 1 */ "WRITE-BACK (CTL)",
  /* 2 */ "WRITE-THROUGH",
  /* 3 */ "WRITE-MODULAR"
};

static const char *cm_names[] = {
  /* 0 */ "SUICIDE",
  /* 1 */ "DELAY",
  /* 2 */ "BACKOFF",
  /* 3 */ "MODULAR"
};

/* Global variables */
global_t _tinystm =
    { .nb_specific = 0
    , .initialized = 0
#ifdef IRREVOCABLE_ENABLED
    , .irrevocable = 0
#endif /* IRREVOCABLE_ENABLED */
    };

/* ################################################################### *
 * TYPES
 * ################################################################### */


/*
 * Transaction nesting is supported in a minimalist way (flat nesting):
 * - When a transaction is started in the context of another
 *   transaction, we simply increment a nesting counter but do not
 *   actually start a new transaction.
 * - The environment to be used for setjmp/longjmp is only returned when
 *   no transaction is active so that it is not overwritten by nested
 *   transactions. This allows for composability as the caller does not
 *   need to know whether it executes inside another transaction.
 * - The commit of a nested transaction simply decrements the nesting
 *   counter. Only the commit of the top-level transaction will actually
 *   carry through updates to shared memory.
 * - An abort of a nested transaction will rollback the top-level
 *   transaction and reset the nesting counter. The call to longjmp will
 *   restart execution before the top-level transaction.
 * Using nested transactions without setjmp/longjmp is not recommended
 * as one would need to explicitly jump back outside of the top-level
 * transaction upon abort of a nested transaction. This breaks
 * composability.
 */

/*
 * Reading from the previous version of locked addresses is implemented
 * by peeking into the write set of the transaction that owns the
 * lock. Each transaction has a unique identifier, updated even upon
 * retry. A special "commit" bit of this identifier is set upon commit,
 * right before writing the values from the redo log to shared memory. A
 * transaction can read a locked address if the identifier of the owner
 * does not change between before and after reading the value and
 * version, and it does not have the commit bit set.
 */

/* ################################################################### *
 * THREAD-LOCAL
 * ################################################################### */

#if defined(TLS_POSIX) || defined(TLS_DARWIN)
/* TODO this may lead to false sharing. */
/* TODO this could be renamed with tinystm prefix */
pthread_key_t thread_tx;
pthread_key_t thread_gc;
#elif defined(TLS_COMPILER)
__thread stm_tx_t* thread_tx = NULL;
__thread long thread_gc = 0;
#endif /* defined(TLS_COMPILER) */

/* ################################################################### *
 * STATIC
 * ################################################################### */

#if CM == CM_MODULAR
/*
 * Kill other.
 */
static int
cm_aggressive(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  return KILL_OTHER;
}

/*
 * Kill self.
 */
static int
cm_suicide(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  return KILL_SELF;
}

/*
 * Kill self and wait before restart.
 */
static int
cm_delay(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  return KILL_SELF | DELAY_RESTART;
}

/*
 * Oldest transaction has priority.
 */
static int
cm_timestamp(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  if (me->timestamp < other->timestamp)
    return KILL_OTHER;
  if (me->timestamp == other->timestamp && (uintptr_t)me < (uintptr_t)other)
    return KILL_OTHER;
  return KILL_SELF | DELAY_RESTART;
}

/*
 * Transaction with more work done has priority.
 */
static int
cm_karma(struct stm_tx *me, struct stm_tx *other, int conflict)
{
  unsigned int me_work, other_work;

  me_work = (me->w_set.nb_entries << 1) + me->r_set.nb_entries;
  other_work = (other->w_set.nb_entries << 1) + other->r_set.nb_entries;

  if (me_work < other_work)
    return KILL_OTHER;
  if (me_work == other_work && (uintptr_t)me < (uintptr_t)other)
    return KILL_OTHER;
  return KILL_SELF;
}

struct {
  const char *name;
  int (*f)(stm_tx_t *, stm_tx_t *, int);
} cms[] = {
  { "aggressive", cm_aggressive },
  { "suicide", cm_suicide },
  { "delay", cm_delay },
  { "timestamp", cm_timestamp },
  { "karma", cm_karma },
  { NULL, NULL }
};
#endif /* CM == CM_MODULAR */

#ifdef SIGNAL_HANDLER
/*
 * Catch signal (to emulate non-faulting load).
 */
static void
signal_catcher(int sig)
{
  sigset_t block_signal;
  stm_tx_t *tx = tls_get_tx();

  /* A fault might only occur upon a load concurrent with a free (read-after-free) */
  PRINT_DEBUG("Caught signal: %d\n", sig);

  /* TODO: TX_KILLED should be also allowed */
  if (tx == NULL || tx->attr.no_retry || GET_STATUS(tx->status) != TX_ACTIVE) {
    /* There is not much we can do: execution will restart at faulty load */
    fprintf(stderr, "Error: invalid memory accessed and no longjmp destination\n");
    exit(1);
  }

  /* Unblock the signal since there is no return to signal handler */
  sigemptyset(&block_signal);
  sigaddset(&block_signal, sig);
  pthread_sigmask(SIG_UNBLOCK, &block_signal, NULL);

  /* Will cause a longjmp */
  stm_rollback(tx, STM_ABORT_SIGNAL);
}
#endif /* SIGNAL_HANDLER */

unsigned long get_total_commits(){
	unsigned long commits = 0;
	stm_tx_t *t;
	t = _tinystm.threads;
	while (t != NULL) {
		commits += t->stat_commits;
		t = t->next;
	}
	return commits;
}

unsigned long get_total_aborts(){
        unsigned long aborts = 0;
        stm_tx_t *t;
        t = _tinystm.threads;
        while (t != NULL) {
                aborts += t->stat_aborts;
                t = t->next;
        }
        return aborts;
}

thread_proc_4(void* x){
	int iterations = 0;
	int tdir = 75;
	int fdir = 1;
	int tunef = 1;
	int skip = 1;
	int thresh_index = 1;
	volatile unsigned long tcommits_pre = 0;
 	volatile unsigned long tcommits_post = 0;
	int tcommits = 0;
	int tcommits_prev = 0;
	int sampling_period = 10000;

	struct timespec tim, tim2;
 	tim.tv_sec = 0;
 	tim.tv_nsec = sampling_period*10000;


 	float power_pre,power_post,power_prev,power_max;
 	float power=0;
 	int j;
 	float edp=0, edp_mae, edp_avg=0;
 	float edp_prev = 0;

	while(running){

		iterations += 1;
	  
		tcommits_pre = get_total_commits();
		nanosleep(&tim , &tim2);
	 	tcommits_post = get_total_commits();
        	tcommits = tcommits_post-tcommits_pre;
 
                printf("current thresholds are %d, %d\n",ferraris, thresh_index);

        	if(!running)
                	break;

	        power_post = 0;
        	for (j = 0; j < nr_packages; j++)
                	power_post += source->get_power(j);
        	power = power_post;

        	if(power==0.0)
                	pthread_exit(NULL);

        	edp = tcommits/power;

		if(tcommits < 30)
			continue;


		if((edp < edp_prev) && !skip){
			if(tunef == 1){
				tdir *= -1;
				if (tdir != 1 && tdir != -1){
	                                tdir = tdir/2;
        		                if (tdir==0)
                        		        tdir = 1;
                        	}
				thresh_index += tdir;
			}
			else{
				fdir *= -1;
				ferraris += fdir;
			}
		}


		if(tunef == 1)
			ferraris += fdir;
		else
			thresh_index += tdir;

		if (thresh_index < 0){
                	thresh_index = 0;
        	}
        	else if (thresh_index > max_backoff_threshold ){
                	thresh_index = max_backoff_threshold;
        	}
		if (ferraris == -1)
                	ferraris = 0;
        	else if (ferraris > max_ferraris )
                	ferraris = max_ferraris;

		spintosleep = backoff_thresholds[thresh_index];
		backoff_threshold = spintosleep*(beta+5);

		edp_prev = edp;
		skip = 0;	
		tunef *= -1;
	}
}


thread_proc_5(void* x){
	int sampling_period = 10;
	struct timespec tim, tim2;
	tim.tv_sec = 0;
	tim.tv_nsec = sampling_period*1000;
	int tcommits=0, taborts=0;
	volatile unsigned long tcommits_pre=0, tcommits_post=0, taborts_pre=0, taborts_post=0;
	int counter = 0;
	while(running){
		taborts_pre = get_total_aborts();
		tcommits_pre = get_total_commits();
        	nanosleep(&tim , &tim2);
		taborts_post = get_total_aborts();
	        tcommits_post = get_total_commits();
        	tcommits = tcommits_post-tcommits_pre;
		taborts = taborts_post-taborts_pre;
		if (tcommits>100){
			counter++;
			if (counter==100){
				printf("commits in past 0 us is: shady\n");
                        	spintosleep=1000;
                        	backoff_threshold = 55*spintosleep;
			}
		}
		//if (tcommits < 10){
                //	continue;
        	//}
		printf("commits in past %d us is: %d,%d\n",sampling_period,tcommits,taborts);	
	}
}

thread_proc(void* x)
{
 int jump = -1;
 int ferraris_prev;
 int can_switch = 1;
 int dir = -1;
 int skip = 0;
 int oscillating = 0;
 int confidence = 0;
 //int confidence_threshold = 1;
 int prev_turn = -3;
 unsigned long min_threshold = 30;
 unsigned long max_threshold = 500;
 unsigned long tcommits_pre=0;
 unsigned long tcommits_post=0;
 int tcommits=0;
 unsigned long sampling_period=10000;

 struct timespec tim, tim2;
 tim.tv_sec = 0;
 tim.tv_nsec = sampling_period*1000;

 unsigned long tcommits_prev=0;
 unsigned long iterations=0;
 unsigned long prev_iterations=-1;

 float power_prev=0;
 float power=0;
 int j;
 float edp;
 float edp_prev = 0;
 /*while(tcommits_prev == 0){
	tcommits_prev=get_total_commits();
 	ulseep(10);	
 }*/
 while(running){
    if(can_switch){
	iterations += 1;
	tcommits_pre = get_total_commits();
	nanosleep(&tim , &tim2);
	tcommits_post = get_total_commits();
	tcommits = tcommits_post-tcommits_pre;

	if(!running)
                break;
		
	power = 0;
        for (j = 0; j < nr_packages; j++)
                power += source->get_power(j);
	edp = tcommits/power;

	printf("ferraris: %d, %f, %d, %f\n",ferraris,edp,tcommits,power);

        
	if (tcommits <= 30)
                continue;
	
	//printf("commits: %d, sampling period: %d\n",tcommits,sampling_period);
	/*if(tcommits < min_threshold){
		sampling_period *= 2;
		skip  = 1;
		//tcommits_prev = tcommits*2;
		continue;
	}
	if(tcommits > max_threshold){
		sampling_period /= 2;
		skip = 1;
		if (sampling_period == 0)
			sampling_period = 1;
		//tcommits_prev = tcommits/2;
		continue;
	}*/


        if(edp < edp_prev && !skip){


		if((abs(ferraris-prev_turn)) <= 2 && abs(ferraris-prev_turn) > 0){
                        oscillating += 1;
                        //printf("oscillating at %d, %d\n",ferraris,prev_turn);
                }
                else
                        oscillating = 0;
                if(oscillating == 3){
                        prev_iterations = iterations;
                        can_switch = 0;
                        oscillating = 0;
                        printf("\n\ncaught oscillating\n\n");
                        break;
                }



	//if(tcommits < tcommits_prev && !skip){
		//confidence++;
		prev_turn = ferraris;
		//if(confidence == 2){
			//confidence_threshold++;
			dir *= -1;
			//confidence == 0;
			//if (dir != 1 || dir != -1)
			//	dir = dir/2;
			//if (dir==0)
			//	dir = 1;
		        //fprintf(stderr,"turning\t%d\t%d\t%d\t%d\n",iterations,tcommits,tcommits_prev,ferraris);
		//}
	}
	//else
	//	confidence = 0;
	tcommits_prev = tcommits;
	edp_prev = edp;
	if(can_switch && !skip)
		ferraris += dir;
	skip = 0;
	if (ferraris == -1){
		prev_turn = 0;
		ferraris = 0;
	}
	else if (ferraris > max_ferraris ){
		ferraris = max_ferraris;
		prev_turn = max_ferraris;
	}
	//asym_threshold += dir;
	//if (asym_threshold == 0){
	//	asym_threshold = 100;
		//printf("zeroing: %d\n",asym_threshold);
	//}
	#if BO==DDASYM_ADPT || BO==DDASYM_SLEEP
 		if (ferraris != 0){
			if (asym_threshold < 50 && can_switch){
				ferraris -= 2;
				//printf("removing two ferraris: %d\n",ferraris); 
				can_switch == 0;
			}
			else if (asym_threshold > 100 && !can_switch)
				can_switch = 1;
			else if (asym_threshold > 200){
				can_switch == 1;
				if (ferraris < 16)
					ferraris += 2;
			}
		}
	#endif
    }
    else{
	//pthread_exit(NULL);

	//break;
	iterations += 1;

	 tcommits_pre = get_total_commits();
        nanosleep(&tim , &tim2);
        tcommits_post = get_total_commits();
        tcommits = tcommits_post-tcommits_pre;

	power = 0;
        for (j = 0; j < nr_packages; j++)
                power += source->get_power(j);

        if(power==0.0)
                pthread_exit(NULL);

        edp = tcommits/power;

	printf("ferraris: %d, %f, %d, %f\n",ferraris,edp,tcommits,power);
        if (iterations> prev_iterations+20 && jump == -1){
                edp_prev=edp;
                jump =  rand() % 17;//(100-thresh_index);
                ferraris_prev = ferraris;
                ferraris = jump;
                skip = 1;
		//can_switch=1;
                printf("try this threshold: %d\n",jump);
        }

        if (iterations> prev_iterations+21){
                jump = -1;
                if(edp<edp_prev){
                        can_switch = 0;
                        ferraris = ferraris_prev;
                        prev_iterations = iterations;
                }
                else
                        can_switch = 1;
        }

    }
}
 

        //for(j = 0; j < nr_packages; j++){
        //        source->fini_device(j);
        //        printf("Power for package %d: %f\n", j,source->get_energy(j));
        //}

   //pthread_exio(NULL);

}

thread_proc_3(void* x)
{
 int jump = -1;
 int can_switch = 1;
 int dir = 75;
 int skip = 0;
 int tunfer = 1;
 int oscillating = 0;
 int confidence = 0;
 int thresh_index_prev,thresh_index_best;
 int prev_turn = -3;
 unsigned long min_threshold = 30;
 unsigned long max_threshold = 200;
 volatile unsigned long tcommits_pre=0;
 volatile unsigned long tcommits_post=0;
 int tcommits=0,tcommits_max;
 unsigned long sampling_period=10000;
 unsigned long tcommits_prev=0;
 unsigned long iterations=-1;
 unsigned long prev_iterations=-1;
	
 struct timespec tim, tim2;
 tim.tv_sec = 0;
 tim.tv_nsec = sampling_period*10000;

 float power_pre,power_post,power_prev,power_max;
 float power=0;
 int j;
 float edp=0, edp_mae, edp_avg=0;
 float edp_prev = 0;
 
while(running){
    if(can_switch){
	iterations += 1;

	tcommits_pre = get_total_commits();	
        nanosleep(&tim , &tim2);
	tcommits_post = get_total_commits();
	tcommits = tcommits_post-tcommits_pre;
	
	if(!running)
		break;
	
	power_post = 0;
        for (j = 0; j < nr_packages; j++)
                power_post += source->get_power(j);
	power = power_post;

	if(power==0.0)
                pthread_exit(NULL);

        edp = tcommits/power;

	//thresholds[iterations] = thresh_index;
	printf("%d: current threshold is %d %f %d %f with step %d\n",iterations, thresh_index,edp,tcommits,power,dir);


	if (tcommits < 30){
		if(jump!=-1)
			skip = 1;
		continue;
	}


        if((edp < edp_prev) && !skip){


               if((abs(thresh_index-prev_turn)) <= 3){
                        oscillating += 1;
                        printf("oscillating at %d, %d\n",thresh_index,prev_turn);
                }
                else
                        oscillating = 0;
                if(oscillating == 5){
                        prev_iterations = iterations;
                        can_switch = 0;
                        oscillating = 0;
                        thresholding = 0;
                        printf("\n\ncaught oscillating\n\n");
                }

		prev_turn = thresh_index;
			dir *= -1;
			if (dir != 1 && dir != -1){
				dir = dir/2;
				if (dir==0)
					dir = 1;
			}
	}

	if(can_switch && !skip){
                thresh_index += dir;
        }


	edp_prev = edp;
	skip = 0;

	if (thresh_index < 0){
		//prev_turn = 0;
		thresh_index = 0;
	}
	else if (thresh_index > max_backoff_threshold ){
		//prev_turn = max_backoff_threshold;
		thresh_index = max_backoff_threshold;
	}
	spintosleep = backoff_thresholds[thresh_index];
	backoff_threshold = 55*spintosleep;  
    }
    else{

	#if BO==DASYM_ADPT
	if(tunfer){
		//thread_proc(NULL);
		tunfer = 0;
	}
	#endif

	//can_switch = 1;
	//continue;

        tcommits_pre = get_total_commits();
        nanosleep(&tim , &tim2);
        tcommits_post = get_total_commits();
        tcommits = tcommits_post-tcommits_pre;

	
        if(!running)
                break;

        power_post = 0;
        for (j = 0; j < nr_packages; j++)
                power_post += source->get_power(j);
        power = power_post;

	if(power==0.0)
		pthread_exit(NULL);
		
        edp = tcommits/power;
        printf("current threshold is %d %f %d %f with step %d\n",thresh_index,edp,tcommits,power,dir);

	if(tcommits<30)
		continue;

	iterations += 1;

	if (iterations> prev_iterations+20 && jump == -1){
		edp_prev=edp;
		jump =  rand() % 100;//(100-thresh_index);
		thresh_index_prev = thresh_index;
		thresh_index = jump;
		spintosleep = backoff_thresholds[thresh_index];
        	backoff_threshold = 55*spintosleep;
		skip = 1;
		//can_switch = 1;
	}

        if (iterations> prev_iterations+21){
		jump = -1;
		//tunfer = 1;
		if(edp<edp_prev){
                        can_switch = 0;
                        thresh_index = thresh_index_prev;
                        spintosleep = backoff_thresholds[thresh_index];
                        backoff_threshold = 55*spintosleep;
                        prev_iterations = iterations;
                }
		else{
			tunfer = 1;
			can_switch = 1;
		}
	}

	//can_switch=1;
    }
  }
   pthread_exit(NULL);
}




thread_proc_2(void* x)
{
	do_measure_all_cpus(100000,0);
	pthread_exit(NULL);
}

/* ################################################################### *
 * STM FUNCTIONS
 * ################################################################### */

/*
 * Called once (from main) to initialize STM infrastructure.
 */
_CALLCONV void
stm_init(void)
{
	int j;
        source = get_available_sources();
        assert(source != NULL);
        nr_packages = source->get_nr_packages();
        printf("Found %d packages.\n", nr_packages);

        for(j = 0; j < nr_packages; j++){
                source->init_device(j);
        }
	pthread_t t2;
        pthread_create(&t2, NULL, thread_proc_2, NULL);
	running = 1;
#if CM == CM_BACKOFF
	thresh_index  = 0;
	spintosleep = atoi(getenv("SPINTOSLEEP"));
	beta = atoi(getenv("BETA"));
	backoff_threshold = spintosleep*(beta+5);
	//pthread_t t3;
        //pthread_create(&t3, NULL, thread_proc_3, NULL);
	//pthread_t t5;
        //pthread_create(&t5, NULL, thread_proc_5, NULL);
	//backoff_threshold = atoi(getenv("THRESHOLD"));
	//spintosleep = 3.0;
	#if BO==ASYM_ADPT || BO==ASYM_SPIN || BO==ASYM_SLEEP || BO==ASYM_ADPT_THRESH || BO==DASYM_SPIN
	max_ferraris = atoi(getenv("FERRARIS"));
	ferraris = max_ferraris;
	asym_threshold = 1000000000;
	srand(time(NULL));
	#elif BO==DASYM_ADPT || BO==DDASYM_ADPT || BO==DASYM_SLEEP || BO==DDASYM_SLEEP || BO==DASYM_SPIN
	max_ferraris = atoi(getenv("FERRARIS"));
	ferraris=max_ferraris/2;
	asym_threshold = 1000000000;
	for(j=0;j<150;j++)
                backoff_thresholds[j] = 1500+j*1000;
        min_backoff_threshold = 0;
        max_backoff_threshold = 149;
	thresholding = 1;
	pthread_t t3;
        pthread_create(&t3, NULL, thread_proc_4, NULL);
	//pthread_t t1;
        //pthread_create(&t1, NULL, thread_proc, NULL);
	#elif BO==ADPT_THRESH || BO==ASYM_ADPT_THRESH
	//backoff_thresholds[0]=1500;
	for(j=0;j<150;j++)
		//backoff_thresholds[j] = backoff_thresholds[j-1]+0.045*backoff_thresholds[j];
		backoff_thresholds[j] = 1500+j*1000;
		//backoff_thresholds[j] = (1UL << j+4);

	min_backoff_threshold = 0;
	max_backoff_threshold = 149;
	//backoff_threshold = backoff_thresholds[15];
	pthread_t t3;
        pthread_create(&t3, NULL, thread_proc_3, NULL);
	#endif
#endif
#if CM == CM_MODULAR
  char *s;
  #if MOD == KARMA
  stm_set_parameter("cm_policy","karma");
  #elif MOD == AGGRESSIVE
  stm_set_parameter("cm_policy","aggressive");
  #elif MOD == TIMESTAMP
  stm_set_parameter("cm_policy","timestamp");
  #elif MOD == SUICIDE
  stm_set_parameter("cm_policy","suicide");
  #endif
#endif /* CM == CM_MODULAR */
#ifdef SIGNAL_HANDLER
  struct sigaction act;
#endif /* SIGNAL_HANDLER */

  PRINT_DEBUG("==> stm_init()\n");

  if (_tinystm.initialized)
    return;

  PRINT_DEBUG("\tsizeof(word)=%d\n", (int)sizeof(stm_word_t));

  PRINT_DEBUG("\tVERSION_MAX=0x%lx\n", (unsigned long)VERSION_MAX);

  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(void *));
  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == sizeof(atomic_t));

#ifdef EPOCH_GC
  gc_init(stm_get_clock);
#endif /* EPOCH_GC */

#if CM == CM_MODULAR
  s = getenv(VR_THRESHOLD);
  if (s != NULL)
    _tinystm.vr_threshold = (int)strtol(s, NULL, 10);
  else
    _tinystm.vr_threshold = VR_THRESHOLD_DEFAULT;
  PRINT_DEBUG("\tVR_THRESHOLD=%d\n", _tinystm.vr_threshold);
#endif /* CM == CM_MODULAR */

  /* Set locks and clock but should be already to 0 */
  memset((void *)_tinystm.locks, 0, LOCK_ARRAY_SIZE * sizeof(stm_word_t));
  CLOCK = 0;

  stm_quiesce_init();

  tls_init();

#ifdef SIGNAL_HANDLER
  if (getenv(NO_SIGNAL_HANDLER) == NULL) {
    /* Catch signals for non-faulting load */
    act.sa_handler = signal_catcher;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGBUS, &act, NULL) < 0 || sigaction(SIGSEGV, &act, NULL) < 0) {
      perror("sigaction");
      exit(1);
    }
  }
#endif /* SIGNAL_HANDLER */
  _tinystm.initialized = 1;
}

/*
 * Called once (from main) to clean up STM infrastructure.
 */
_CALLCONV void
stm_exit(void)
{
	//#if BO==ASYM_ADPT || BO==ASYM_SPIN || BO==ASYM_SLEEP
	running = 0;
	int x,y;
	for (x=0;x<64;x++){
		fprintf(stderr,"cpu %d:\t",x);
		for (y=0;y<7;y++){
			fprintf(stderr,"%d\t",freqmonitor[x][y]);
		}
		fprintf(stderr,"\n");
	}
	//#endif
	usleep(2);
	int j;
        for(j = 0; j < nr_packages; j++){
                source->fini_device(j);
                printf("Power for package %d: %f\n", j,source->get_energy(j));
        }

  PRINT_DEBUG("==> stm_exit()\n");

  if (!_tinystm.initialized)
    return;

  tls_exit();
  stm_quiesce_exit();

#ifdef EPOCH_GC
  gc_exit();
#endif /* EPOCH_GC */

  _tinystm.initialized = 0;
}

/*
 * Called by the CURRENT thread to initialize thread-local STM data.
 */
_CALLCONV stm_tx_t *
stm_init_thread(void)
{
  return int_stm_init_thread();
}

/*
 * Called by the CURRENT thread to cleanup thread-local STM data.
 */
_CALLCONV void
stm_exit_thread(void)
{
  TX_GET;
  int_stm_exit_thread(tx);
}

_CALLCONV void
stm_exit_thread_tx(stm_tx_t *tx)
{
  int_stm_exit_thread(tx);
}

/*
 * Called by the CURRENT thread to start a transaction.
 */
_CALLCONV sigjmp_buf *
stm_start(stm_tx_attr_t attr)
{
  //stick_this_thread_to_core(-1);
  TX_GET;
  return int_stm_start(tx, attr);
}

_CALLCONV sigjmp_buf *
stm_start_tx(stm_tx_t *tx, stm_tx_attr_t attr)
{
  return int_stm_start(tx, attr);
}

/*
 * Called by the CURRENT thread to commit a transaction.
 */
_CALLCONV int
stm_commit(void)
{
  TX_GET;
  return int_stm_commit(tx);
}

_CALLCONV int
stm_commit_tx(stm_tx_t *tx)
{
  return int_stm_commit(tx);
}

/*
 * Called by the CURRENT thread to abort a transaction.
 */
_CALLCONV void
stm_abort(int reason)
{
  TX_GET;
  stm_rollback(tx, reason | STM_ABORT_EXPLICIT);
}

_CALLCONV void
stm_abort_tx(stm_tx_t *tx, int reason)
{
  stm_rollback(tx, reason | STM_ABORT_EXPLICIT);
}

/*
 * Called by the CURRENT thread to load a word-sized value.
 */
_CALLCONV ALIGNED stm_word_t
stm_load(volatile stm_word_t *addr)
{
  TX_GET;
  return int_stm_load(tx, addr);
}

_CALLCONV stm_word_t
stm_load_tx(stm_tx_t *tx, volatile stm_word_t *addr)
{
  return int_stm_load(tx, addr);
}

/*
 * Called by the CURRENT thread to store a word-sized value.
 */
_CALLCONV ALIGNED void
stm_store(volatile stm_word_t *addr, stm_word_t value)
{
  TX_GET;
  int_stm_store(tx, addr, value);
}

_CALLCONV void
stm_store_tx(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value)
{
  int_stm_store(tx, addr, value);
}

/*
 * Called by the CURRENT thread to store part of a word-sized value.
 */
_CALLCONV ALIGNED void
stm_store2(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  TX_GET;
  int_stm_store2(tx, addr, value, mask);
}

_CALLCONV void
stm_store2_tx(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  int_stm_store2(tx, addr, value, mask);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
_CALLCONV int
stm_active(void)
{
  TX_GET;
  return int_stm_active(tx);
}

_CALLCONV int
stm_active_tx(stm_tx_t *tx)
{
  return int_stm_active(tx);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
_CALLCONV int
stm_aborted(void)
{
  TX_GET;
  return int_stm_aborted(tx);
}

_CALLCONV int
stm_aborted_tx(stm_tx_t *tx)
{
  return int_stm_aborted(tx);
}

/*
 * Called by the CURRENT thread to inquire about the status of a transaction.
 */
_CALLCONV int
stm_irrevocable(void)
{
  TX_GET;
  return int_stm_irrevocable(tx);
}

_CALLCONV int
stm_irrevocable_tx(stm_tx_t *tx)
{
  return int_stm_irrevocable(tx);
}

/*
 * Called by the CURRENT thread to obtain an environment for setjmp/longjmp.
 */
_CALLCONV sigjmp_buf *
stm_get_env(void)
{
  TX_GET;
  return int_stm_get_env(tx);
}

_CALLCONV sigjmp_buf *
stm_get_env_tx(stm_tx_t *tx)
{
  return int_stm_get_env(tx);
}

/*
 * Get transaction attributes.
 */
_CALLCONV stm_tx_attr_t
stm_get_attributes(void)
{
  TX_GET;
  assert (tx != NULL);
  return tx->attr;
}

/*
 * Get transaction attributes from a specifc transaction.
 */
_CALLCONV stm_tx_attr_t
stm_get_attributes_tx(struct stm_tx *tx)
{
  assert (tx != NULL);
  return tx->attr;
}

/*
 * Return statistics about a thread/transaction.
 */
_CALLCONV int
stm_get_stats(const char *name, void *val)
{
  TX_GET;
  return int_stm_get_stats(tx, name, val);
}

_CALLCONV int
stm_get_stats_tx(stm_tx_t *tx, const char *name, void *val)
{
  return int_stm_get_stats(tx, name, val);
}

/*
 * Return STM parameters.
 */
_CALLCONV int
stm_get_parameter(const char *name, void *val)
{
  if (strcmp("contention_manager", name) == 0) {
    *(const char **)val = cm_names[CM];
    return 1;
  }
  if (strcmp("design", name) == 0) {
    *(const char **)val = design_names[DESIGN];
    return 1;
  }
  if (strcmp("initial_rw_set_size", name) == 0) {
    *(int *)val = RW_SET_SIZE;
    return 1;
  }
#if CM == CM_BACKOFF
  if (strcmp("min_backoff", name) == 0) {
    *(unsigned long *)val = MIN_BACKOFF;
    return 1;
  }
  if (strcmp("max_backoff", name) == 0) {
    *(unsigned long *)val = MAX_BACKOFF;
    return 1;
  }
#endif /* CM == CM_BACKOFF */
#if CM == CM_MODULAR
  if (strcmp("vr_threshold", name) == 0) {
    *(int *)val = _tinystm.vr_threshold;
    return 1;
  }
#endif /* CM == CM_MODULAR */
#ifdef COMPILE_FLAGS
  if (strcmp("compile_flags", name) == 0) {
    *(const char **)val = XSTR(COMPILE_FLAGS);
    return 1;
  }
#endif /* COMPILE_FLAGS */
  return 0;
}

/*
 * Set STM parameters.
 */
_CALLCONV int
stm_set_parameter(const char *name, void *val)
{
#if CM == CM_MODULAR
  int i;

  if (strcmp("cm_policy", name) == 0) {
    for (i = 0; cms[i].name != NULL; i++) {
      if (strcasecmp(cms[i].name, (const char *)val) == 0) {
        _tinystm.contention_manager = cms[i].f;
        return 1;
      }
    }
    return 0;
  }
  if (strcmp("cm_function", name) == 0) {
    _tinystm.contention_manager = (int (*)(stm_tx_t *, stm_tx_t *, int))val;
    return 1;
  }
  if (strcmp("vr_threshold", name) == 0) {
    _tinystm.vr_threshold = *(int *)val;
    return 1;
  }
#endif /* CM == CM_MODULAR */
  return 0;
}

/*
 * Create transaction-specific data (return -1 on error).
 */
_CALLCONV int
stm_create_specific(void)
{
  if (_tinystm.nb_specific >= MAX_SPECIFIC) {
    fprintf(stderr, "Error: maximum number of specific slots reached\n");
    return -1;
  }
  return _tinystm.nb_specific++;
}

/*
 * Store transaction-specific data.
 */
_CALLCONV void
stm_set_specific(int key, void *data)
{
  int_stm_set_specific(key, data);
}

/*
 * Fetch transaction-specific data.
 */
_CALLCONV void *
stm_get_specific(int key)
{
  return int_stm_get_specific(key);
}

/*
 * Register callbacks for an external module (must be called before creating transactions).
 */
_CALLCONV int
stm_register(void (*on_thread_init)(void *arg),
             void (*on_thread_exit)(void *arg),
             void (*on_start)(void *arg),
             void (*on_precommit)(void *arg),
             void (*on_commit)(void *arg),
             void (*on_abort)(void *arg),
             void *arg)
{
  if ((on_thread_init != NULL && _tinystm.nb_init_cb >= MAX_CB) ||
      (on_thread_exit != NULL && _tinystm.nb_exit_cb >= MAX_CB) ||
      (on_start != NULL && _tinystm.nb_start_cb >= MAX_CB) ||
      (on_precommit != NULL && _tinystm.nb_precommit_cb >= MAX_CB) ||
      (on_commit != NULL && _tinystm.nb_commit_cb >= MAX_CB) ||
      (on_abort != NULL && _tinystm.nb_abort_cb >= MAX_CB)) {
    fprintf(stderr, "Error: maximum number of modules reached\n");
    return 0;
  }
  /* New callback */
  if (on_thread_init != NULL) {
    _tinystm.init_cb[_tinystm.nb_init_cb].f = on_thread_init;
    _tinystm.init_cb[_tinystm.nb_init_cb++].arg = arg;
  }
  /* Delete callback */
  if (on_thread_exit != NULL) {
    _tinystm.exit_cb[_tinystm.nb_exit_cb].f = on_thread_exit;
    _tinystm.exit_cb[_tinystm.nb_exit_cb++].arg = arg;
  }
  /* Start callback */
  if (on_start != NULL) {
    _tinystm.start_cb[_tinystm.nb_start_cb].f = on_start;
    _tinystm.start_cb[_tinystm.nb_start_cb++].arg = arg;
  }
  /* Pre-commit callback */
  if (on_precommit != NULL) {
    _tinystm.precommit_cb[_tinystm.nb_precommit_cb].f = on_precommit;
    _tinystm.precommit_cb[_tinystm.nb_precommit_cb++].arg = arg;
  }
  /* Commit callback */
  if (on_commit != NULL) {
    _tinystm.commit_cb[_tinystm.nb_commit_cb].f = on_commit;
    _tinystm.commit_cb[_tinystm.nb_commit_cb++].arg = arg;
  }
  /* Abort callback */
  if (on_abort != NULL) {
    _tinystm.abort_cb[_tinystm.nb_abort_cb].f = on_abort;
    _tinystm.abort_cb[_tinystm.nb_abort_cb++].arg = arg;
  }

  return 1;
}

/*
 * Called by the CURRENT thread to load a word-sized value in a unit transaction.
 */
_CALLCONV stm_word_t
stm_unit_load(volatile stm_word_t *addr, stm_word_t *timestamp)
{
#ifdef UNIT_TX
  volatile stm_word_t *lock;
  stm_word_t l, l2, value;

  PRINT_DEBUG2("==> stm_unit_load(a=%p)\n", addr);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Read lock, value, lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (LOCK_GET_OWNED(l)) {
    /* Locked: wait until lock is free */
#ifdef WAIT_YIELD
    sched_yield();
#endif /* WAIT_YIELD */
    goto restart;
  }
  /* Not locked */
  value = ATOMIC_LOAD_ACQ(addr);
  l2 = ATOMIC_LOAD_ACQ(lock);
  if (l != l2) {
    l = l2;
    goto restart_no_load;
  }

  if (timestamp != NULL)
    *timestamp = LOCK_GET_TIMESTAMP(l);

  return value;
#else /* ! UNIT_TX */
  fprintf(stderr, "Unit transaction is not enabled\n");
  exit(-1);
  return 1;
#endif /* ! UNIT_TX */
}

/*
 * Store a word-sized value in a unit transaction.
 */
static INLINE int
stm_unit_write(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask, stm_word_t *timestamp)
{
#ifdef UNIT_TX
  volatile stm_word_t *lock;
  stm_word_t l;

  PRINT_DEBUG2("==> stm_unit_write(a=%p,d=%p-%lu,m=0x%lx)\n",
               addr, (void *)value, (unsigned long)value, (unsigned long)mask);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Try to acquire lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
  if (LOCK_GET_OWNED(l)) {
    /* Locked: wait until lock is free */
#ifdef WAIT_YIELD
    sched_yield();
#endif /* WAIT_YIELD */
    goto restart;
  }
  /* Not locked */
  if (timestamp != NULL && LOCK_GET_TIMESTAMP(l) > *timestamp) {
    /* Return current timestamp */
    *timestamp = LOCK_GET_TIMESTAMP(l);
    return 0;
  }
  /* TODO: would need to store thread ID to be able to kill it (for wait freedom) */
  if (ATOMIC_CAS_FULL(lock, l, LOCK_UNIT) == 0)
    goto restart;
  ATOMIC_STORE(addr, value);
  /* Update timestamp with newer value (may exceed VERSION_MAX by up to MAX_THREADS) */
  l = FETCH_INC_CLOCK + 1;
  if (timestamp != NULL)
    *timestamp = l;
  /* Make sure that lock release becomes visible */
  ATOMIC_STORE_REL(lock, LOCK_SET_TIMESTAMP(l));
  if (unlikely(l >= VERSION_MAX)) {
    /* Block all transactions and reset clock (current thread is not in active transaction) */
    stm_quiesce_barrier(NULL, rollover_clock, NULL);
  }
  return 1;
#else /* ! UNIT_TX */
  fprintf(stderr, "Unit transaction is not enabled\n");
  exit(-1);
  return 1;
#endif /* ! UNIT_TX */
}

/*
 * Called by the CURRENT thread to store a word-sized value in a unit transaction.
 */
_CALLCONV int
stm_unit_store(volatile stm_word_t *addr, stm_word_t value, stm_word_t *timestamp)
{
  return stm_unit_write(addr, value, ~(stm_word_t)0, timestamp);
}

/*
 * Called by the CURRENT thread to store part of a word-sized value in a unit transaction.
 */
_CALLCONV int
stm_unit_store2(volatile stm_word_t *addr, stm_word_t value, stm_word_t mask, stm_word_t *timestamp)
{
  return stm_unit_write(addr, value, mask, timestamp);
}

/*
 * Enable or disable extensions and set upper bound on snapshot.
 */
static INLINE void
int_stm_set_extension(stm_tx_t *tx, int enable, stm_word_t *timestamp)
{
#ifdef UNIT_TX
  tx->attr.no_extend = !enable;
  if (timestamp != NULL && *timestamp < tx->end)
    tx->end = *timestamp;
#else /* ! UNIT_TX */
  fprintf(stderr, "Unit transaction is not enabled\n");
  exit(-1);
#endif /* ! UNIT_TX */
}

_CALLCONV void
stm_set_extension(int enable, stm_word_t *timestamp)
{
  TX_GET;
  int_stm_set_extension(tx, enable, timestamp);
}

_CALLCONV void
stm_set_extension_tx(stm_tx_t *tx, int enable, stm_word_t *timestamp)
{
  int_stm_set_extension(tx, enable, timestamp);
}

/*
 * Get curent value of global clock.
 */
_CALLCONV stm_word_t
stm_get_clock(void)
{
  return GET_CLOCK;
}

/*
 * Get current transaction descriptor.
 */
_CALLCONV stm_tx_t *
stm_current_tx(void)
{
  return tls_get_tx();
}

/* ################################################################### *
 * UNDOCUMENTED STM FUNCTIONS (USE WITH CARE!)
 * ################################################################### */

#ifdef CONFLICT_TRACKING
/*
 * Get thread identifier of other transaction.
 */
_CALLCONV int
stm_get_thread_id(stm_tx_t *tx, pthread_t *id)
{
  *id = tx->thread_id;
  return 1;
}

/*
 * Set global conflict callback.
 */
_CALLCONV int
stm_set_conflict_cb(void (*on_conflict)(stm_tx_t *tx1, stm_tx_t *tx2))
{
  _tinystm.conflict_cb = on_conflict;
  return 1;
}
#endif /* CONFLICT_TRACKING */

/*
 * Set the CURRENT transaction as irrevocable.
 */
static INLINE int
int_stm_set_irrevocable(stm_tx_t *tx, int serial)
{
#ifdef IRREVOCABLE_ENABLED
# if CM == CM_MODULAR
  stm_word_t t;
# endif /* CM == CM_MODULAR */

  if (!IS_ACTIVE(tx->status) && serial != -1) {
    /* Request irrevocability outside of a transaction or in abort handler (for next execution) */
    tx->irrevocable = 1 + (serial ? 0x08 : 0);
    return 0;
  }

  /* Are we already in irrevocable mode? */
  if ((tx->irrevocable & 0x07) == 3) {
    return 1;
  }

  if (tx->irrevocable == 0) {
    /* Acquire irrevocability for the first time */
    tx->irrevocable = 1 + (serial ? 0x08 : 0);
    /* Try acquiring global lock */
    if (_tinystm.irrevocable == 1 || ATOMIC_CAS_FULL(&_tinystm.irrevocable, 0, 1) == 0) {
      /* Transaction will acquire irrevocability after rollback */
      stm_rollback(tx, STM_ABORT_IRREVOCABLE);
      return 0;
    }
    /* Success: remember we have the lock */
    tx->irrevocable++;
    /* Try validating transaction */
printf(DESIGN);
#if DESIGN == WRITE_BACK_ETL
    if (!stm_wbetl_validate(tx)) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#elif DESIGN == WRITE_BACK_CTL
    if (!stm_wbctl_validate(tx)) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#elif DESIGN == WRITE_THROUGH
    if (!stm_wt_validate(tx)) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#elif DESIGN == MODULAR
    if ((tx->attr.id == WRITE_BACK_CTL && stm_wbctl_validate(tx))
       || (tx->attr.id == WRITE_THROUGH && stm_wt_validate(tx))
       || (tx->attr.id != WRITE_BACK_CTL && tx->attr.id != WRITE_THROUGH && stm_wbetl_validate(tx))) {
      stm_rollback(tx, STM_ABORT_VALIDATE);
      return 0;
    }
#endif /* DESIGN == MODULAR */

# if CM == CM_MODULAR
   /* We might still abort if we cannot set status (e.g., we are being killed) */
    t = tx->status;
    if (GET_STATUS(t) != TX_ACTIVE || ATOMIC_CAS_FULL(&tx->status, t, t + (TX_IRREVOCABLE - TX_ACTIVE)) == 0) {
      stm_rollback(tx, STM_ABORT_KILLED);
      return 0;
    }
# endif /* CM == CM_MODULAR */
    if (serial && tx->w_set.nb_entries != 0) {
      /* TODO: or commit the transaction when we have the irrevocability. */
      /* Don't mix transactional and direct accesses => restart with direct accesses */
      stm_rollback(tx, STM_ABORT_IRREVOCABLE);
      return 0;
    }
  } else if ((tx->irrevocable & 0x07) == 1) {
    /* Acquire irrevocability after restart (no need to validate) */
    while (_tinystm.irrevocable == 1 || ATOMIC_CAS_FULL(&_tinystm.irrevocable, 0, 1) == 0)
      ;
    /* Success: remember we have the lock */
    tx->irrevocable++;
  }
  assert((tx->irrevocable & 0x07) == 2);

  /* Are we in serial irrevocable mode? */
  if ((tx->irrevocable & 0x08) != 0) {
    /* Stop all other threads */
    if (stm_quiesce(tx, 1) != 0) {
      /* Another thread is quiescing and we are active (trying to acquire irrevocability) */
      assert(serial != -1);
      stm_rollback(tx, STM_ABORT_IRREVOCABLE);
      return 0;
    }
  }

  /* We are in irrevocable mode */
  tx->irrevocable++;

#else /* ! IRREVOCABLE_ENABLED */
  fprintf(stderr, "Irrevocability is not supported in this configuration\n");
  exit(-1);
#endif /* ! IRREVOCABLE_ENABLED */
  return 1;
}

_CALLCONV NOINLINE int
stm_set_irrevocable(int serial)
{
  TX_GET;
  return int_stm_set_irrevocable(tx, serial);
}

_CALLCONV NOINLINE int
stm_set_irrevocable_tx(stm_tx_t *tx, int serial)
{
  return int_stm_set_irrevocable(tx, serial);
}

/*
 * Increment the value of global clock (Only for TinySTM developers).
 */
void
stm_inc_clock(void)
{
  FETCH_INC_CLOCK;
}

