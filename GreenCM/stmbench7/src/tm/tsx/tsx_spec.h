#include "../../thread/pthread_wrap.h"
#include <stdint.h>

namespace sb7 {
	extern pthread_mutex_t global_lock;
	extern pthread_mutex_t aux_lock;
}

inline void sb7::global_init_tm() {
	pthread_mutex_init(&(global_lock), NULL);
	pthread_mutex_init(&(aux_lock), NULL);
}

inline void sb7::thread_init_tm() {
}

inline void sb7::global_clean_tm() {
}

inline void sb7::thread_clean_tm() {
}

inline void* sb7::tm_read_word(void* addr) {
	return (void*)*((intptr_t*)addr);
}

inline void sb7::tm_write_word(void* addr, void* val) {
	*((intptr_t*)addr) = (intptr_t)val;
}
