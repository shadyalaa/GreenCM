#include <cmath>
#include <setjmp.h>

#include "pthread_wrap.h"
#include "../tm/tm_spec.h"
#include "../common/time.h"
#include "../common/memory.h"
#include "../tm/tm_tx.h"
#include "thread.h"
#include "thread_fun.h"

#include "../parameters.h"
#include "../data_holder.h"
#include "../sb7_exception.h"
#include "../helpers.h"

namespace sb7 {
	static void *init_single_tx(void *data) {
		DataHolder *dataHolder = (DataHolder *)data;
		dataHolder->init();
		return NULL;
	}
}

void *sb7::init_data_holder(void *data) {
	thread_init(-1);

	if(parameters.shouldInitSingleTx()) {
		run_tx(init_single_tx, 0, data);
	} else {
		DataHolder *dataHolder = (DataHolder *)data;
		dataHolder->initTx();
	}

	// finish up this thread
	thread_clean();

	// just return something
	return NULL;
}

#ifdef STM_WLPDSTM
void *sb7::worker_thread(void *data) {
	WorkerThreadData *wtdata = (WorkerThreadData *)data;
	thread_init(wtdata->threadId);

	while(!wtdata->stopped) {
		int opind = wtdata->getOperationRndInd();
		const Operation *op = wtdata->operations->getOperations()[opind];

		// get start time
		long start_time = get_time_ms();

		// execute transaction
		unsigned start_flag = sigsetjmp(*wlpdstm_get_long_jmp_buf(), 0);

		if(start_flag != LONG_JMP_ABORT_FLAG) {
			// count aborts
			if(start_flag == LONG_JMP_RESTART_FLAG) {
				mem_tx_abort();
				obj_log_tx_abort();

				wtdata->aborted_ops[opind]++;
			}

			mem_tx_start();
			wlpdstm_start_tx();

			try {
				// transaction body
				op->run();
			} catch (Sb7Exception) {
				wlpdstm_abort_tx();
			}

			wlpdstm_commit_tx();

			mem_tx_commit();
			obj_log_tx_commit();
		} else {
			// failed and aborted
			mem_tx_abort();
			obj_log_tx_abort();
				
			wtdata->failed_ops[opind]++;

			// skip this calculation below
			continue;
		}

		// get end time
		long end_time = get_time_ms();

		wtdata->successful_ops[opind]++;
		long ttc = end_time - start_time;

		if(ttc <= wtdata->max_low_ttc) {
			wtdata->operations_ttc[opind][ttc]++;
		} else {
			double logHighTtc = (::log(ttc) - wtdata->max_low_ttc_log) /
				wtdata->high_ttc_log_base;
			int intLogHighTtc =
				MIN((int)logHighTtc, wtdata->high_ttc_entries - 1);
			wtdata->operations_high_ttc_log[opind][intLogHighTtc]++;
		}
	}

	thread_clean();

	// just return something
	return NULL;
}

// PF: START
#elif defined(STM_TINY_STM)
void *sb7::worker_thread(void *data) {
	WorkerThreadData *wtdata = (WorkerThreadData *)data;
	thread_init(wtdata->threadId);

	bool hintRo = parameters.shouldHintRo();

	while(!wtdata->stopped) {
		int opind = wtdata->getOperationRndInd();
		const Operation *op = wtdata->operations->getOperations()[opind];

		// check if operation is read only
		stm_tx_attr_t _a;
		_a.id = 0;
		_a.read_only = hintRo && op->isReadOnly();
		_a.visible_reads = 0;
		_a.no_retry = 0;
		_a.no_extend = 1;

		// get start time
		long start_time = get_time_ms();

		sigjmp_buf *_e = stm_start(_a);
		if (_e == NULL) {
		    std::cout << "Nesting detected, this cannot happen!" << std::endl;
		    exit(1);
		}
        int status = sigsetjmp(*_e, 0);

        // count aborts
        if(status != 0) {
            mem_tx_abort();
            obj_log_tx_abort();

            wtdata->aborted_ops[opind]++;
        }

        mem_tx_start();

        try {
            // transaction body
            op->run();
        } catch (Sb7Exception) {
            ::stm_abort(STM_ABORT_NO_RETRY);
            // failed and aborted
            mem_tx_abort();
            obj_log_tx_abort();

            wtdata->failed_ops[opind]++;

            // skip this calculation below
            continue;
        }

        ::stm_commit();

        mem_tx_commit();
        obj_log_tx_commit();

		// get end time
		long end_time = get_time_ms();

		wtdata->successful_ops[opind]++;
		long ttc = end_time - start_time;

		if(ttc <= wtdata->max_low_ttc) {
			wtdata->operations_ttc[opind][ttc]++;
		} else {
			double logHighTtc = (::log(ttc) - wtdata->max_low_ttc_log) /
				wtdata->high_ttc_log_base;
			int intLogHighTtc =
				MIN((int)logHighTtc, wtdata->high_ttc_entries - 1);
			wtdata->operations_high_ttc_log[opind][intLogHighTtc]++;
		}
	}

#ifdef STM_TINY_STM_DBG
	int nb_aborts;
	stm_get_parameter(tx, "nb_aborts", &nb_aborts);

	if(nb_aborts != 0) {
		std::cout << "Transaction aborted " << nb_aborts
			<< " times" << std::endl;
	}
#endif

	thread_clean();

	// just return something
	return NULL;
}
// PF: END
#elif defined STM_TSX

namespace std {
	#include <immintrin.h>
	#include <rtmintrin.h>
}

# define IS_LOCKED(lock) 		*((int*)(&lock)) != 0

void *sb7::worker_thread(void *data) {
	WorkerThreadData *wtdata = (WorkerThreadData *)data;
	thread_init(wtdata->threadId);

	bool hintRo = parameters.shouldHintRo();

	while(!wtdata->stopped) {
		int opind = wtdata->getOperationRndInd();
		const Operation *op = wtdata->operations->getOperations()[opind];

		// get start time
		long start_time = get_time_ms();

		int tries = 4;
		bool skipTx = false;
		while (1) {
			int status = _xbegin();
			if (status == _XBEGIN_STARTED) {
				if (IS_LOCKED(global_lock)) {
					_xabort(30);
				} else {
					break;
				}
			} else if (status & _XABORT_EXPLICIT && _XABORT_CODE(status) == 31) {
				if (tries <= 2) {
					pthread_mutex_unlock(&aux_lock);
				}
				skipTx = true;
				wtdata->failed_ops[opind]++;
				break;
			} else {
				// mem_tx_abort();
				// obj_log_tx_abort();
				wtdata->aborted_ops[opind]++;

				tries--;
				if (tries == 2) {
					pthread_mutex_lock(&aux_lock);
				}
				else if (tries <= 0) {
					pthread_mutex_lock(&global_lock);
					break;
				}
			}
		}

		if (skipTx) {
			continue;
		}

        try {
            // transaction body
            op->run();
        } catch (Sb7Exception) {
            if (tries > 0) {
            	_xabort(31);
            } else {
            	pthread_mutex_unlock(&global_lock);
            	pthread_mutex_unlock(&aux_lock);
            }
            continue;
        }

        if (tries > 0) {
        	_xend();
        	if (tries <= 2) {
        		pthread_mutex_unlock(&aux_lock);
        	}
        } else {
        	pthread_mutex_unlock(&global_lock);
        	pthread_mutex_unlock(&aux_lock);
        }

		// get end time
		long end_time = get_time_ms();

		wtdata->successful_ops[opind]++;
		long ttc = end_time - start_time;

		if(ttc <= wtdata->max_low_ttc) {
			wtdata->operations_ttc[opind][ttc]++;
		} else {
			double logHighTtc = (::log(ttc) - wtdata->max_low_ttc_log) /
				wtdata->high_ttc_log_base;
			int intLogHighTtc =
				MIN((int)logHighTtc, wtdata->high_ttc_entries - 1);
			wtdata->operations_high_ttc_log[opind][intLogHighTtc]++;
		}
	}
	thread_clean();
	// just return something
	return NULL;
}
#elif defined STM_SEQ
void *sb7::worker_thread(void *data) {
	WorkerThreadData *wtdata = (WorkerThreadData *)data;
	thread_init(wtdata->threadId);

	bool hintRo = parameters.shouldHintRo();

	while(!wtdata->stopped) {
		int opind = wtdata->getOperationRndInd();
		const Operation *op = wtdata->operations->getOperations()[opind];

		// get start time
		long start_time = get_time_ms();

        try {
            // transaction body
            op->run();
        } catch (Sb7Exception) {
            wtdata->failed_ops[opind]++;
            continue;
        }

		// get end time
		long end_time = get_time_ms();

		wtdata->successful_ops[opind]++;
		long ttc = end_time - start_time;

		if(ttc <= wtdata->max_low_ttc) {
			wtdata->operations_ttc[opind][ttc]++;
		} else {
			double logHighTtc = (::log(ttc) - wtdata->max_low_ttc_log) /
				wtdata->high_ttc_log_base;
			int intLogHighTtc =
				MIN((int)logHighTtc, wtdata->high_ttc_entries - 1);
			wtdata->operations_high_ttc_log[opind][intLogHighTtc]++;
		}
	}
	thread_clean();
	// just return something
	return NULL;
}
#else
void *sb7::worker_thread(void *data) {
	WorkerThreadData *wtdata = (WorkerThreadData *)data;
	thread_init(wtdata->threadId);

	TX_DATA;
	bool hintRo = parameters.shouldHintRo();

	while(!wtdata->stopped) {
		int opind = wtdata->getOperationRndInd();
		const Operation *op = wtdata->operations->getOperations()[opind];

		// check if operation is read only
		int ro_flag = (hintRo && op->isReadOnly());
		volatile bool failed = false;
		volatile bool abort = false;
		try {
			// get start time
			long start_time = get_time_ms();

			//////////////////////
			// start of tx code //
			//////////////////////

			volatile bool first = true;
			jmp_buf buf;
			sigsetjmp(buf, 1);

			// deal with failed transactions in this manner
			if(failed) {
				failed = false;
				// perform abort cleanup
				mem_tx_abort();
				obj_log_tx_abort();
				continue;
			}

			// this is a small hack to distinguish between an abort
			// and first run
			if(!first) {
				wtdata->aborted_ops[opind]++;

				// perform abort cleanup
				mem_tx_abort();
				obj_log_tx_abort();
			} else {
				first = false;
			}

			// start a transaction
			mem_tx_start();
			TX_START;
			// perform actual operation
			op->run();

			// try to comit
			TX_COMMIT;

			// if commit was successful we are here
			// and need to cleanup
			mem_tx_commit();
			obj_log_tx_commit();

			////////////////////
			// end of tx code //
			////////////////////

			// get end time
			long end_time = get_time_ms();

			wtdata->successful_ops[opind]++;
			int ttc = (int)(end_time - start_time);

			if(ttc <= wtdata->max_low_ttc) {
				wtdata->operations_ttc[opind][ttc]++;
			} else {
				double logHighTtc = (::log(ttc) - wtdata->max_low_ttc_log) /
					wtdata->high_ttc_log_base;
				int intLogHighTtc =
					MIN((int)logHighTtc, wtdata->high_ttc_entries - 1);
				wtdata->operations_high_ttc_log[opind][intLogHighTtc]++;
			}
		} catch (Sb7Exception) {
			wtdata->failed_ops[opind]++;
			failed = true;
			abort = true;
		}

		// do it like this in order to free exception memory
		if(abort) {
			abort = false;
			TX_ABORT;
		}
	}

	thread_clean();

	// just return something
	return NULL;
}
#endif // STM_WLPDSTM

int sb7::WorkerThreadData::getOperationRndInd() const {
	double oprnd = get_random()->nextDouble();
	const std::vector<double> &opRat = operations->getOperationCdf();
	int opind = 0;

	while(opRat[opind] < oprnd) {
		opind++;
	}

	return opind;
}



