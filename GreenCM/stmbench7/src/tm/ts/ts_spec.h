#include "stm.h"
#include <mod_mem.h>
//#include <mod_stats.h>

inline void sb7::global_init_tm() {
	::stm_init();
	::mod_mem_init(0);
}

inline void sb7::thread_init_tm() {
	::stm_init_thread();
}

inline void sb7::global_clean_tm() {
	::stm_exit();
}

inline void sb7::thread_clean_tm() {
	::stm_exit_thread();
}

inline void *sb7::tm_read_word(void *addr) {
	return (void *)stm_load((volatile stm_word_t *)addr);
}

inline void sb7::tm_write_word(void *addr, void *val) {
	stm_store((volatile stm_word_t *)addr, (stm_word_t)val);
}
