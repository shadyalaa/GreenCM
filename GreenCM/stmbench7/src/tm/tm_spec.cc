#ifdef STM_TL2
#include "tl2/tl2_spec.cc"
#elif defined STM_TINY_STM
#include "ts/ts_spec.cc"
#elif defined STM_WLPDSTM
#include "wlpdstm/wlpdstm_spec.cc"
#elif defined STM_TSX
#include "tsx/tsx_spec.cc"
#elif defined STM_SEQ
#include "seq/seq_spec.cc"
#endif
