#include <stdint.h>

inline void sb7::global_init_tm() {
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
