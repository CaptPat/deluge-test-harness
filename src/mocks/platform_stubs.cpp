// Platform stubs for firmware code compiled on x86.
// Provides implementations for functions declared in definitions.h and
// fault_handler.h that reference ARM hardware or debug infrastructure.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {

void freezeWithError(char const* errmsg) {
	fprintf(stderr, "FREEZE_WITH_ERROR: %s\n", errmsg);
	abort();
}

void fault_handler_print_freeze_pointers(uint32_t addrSYSLR, uint32_t addrSYSSP, uint32_t addrUSRLR,
                                         uint32_t addrUSRSP) {
	(void)addrSYSLR;
	(void)addrSYSSP;
	(void)addrUSRLR;
	(void)addrUSRSP;
}

void handle_cpu_fault(uint32_t addrSYSLR, uint32_t addrSYSSP, uint32_t addrUSRLR, uint32_t addrUSRSP) {
	(void)addrSYSLR;
	(void)addrSYSSP;
	(void)addrUSRLR;
	(void)addrUSRSP;
}

// Phase 9: NEON FM kernel stub — fm_op_kernel.cpp unconditionally #define HAVE_NEON
// and references this extern. The NEON path is never taken in tests (neon=false).
void neon_fm_kernel(const int32_t*, const int32_t*, int32_t*, int, int32_t, int32_t, int32_t, int32_t) {}

} // extern "C"

// Global buffers referenced by firmware utility code
char miscStringBuffer[256] = {};

// Phase 12: spareRenderingBuffer — used by delay.cpp as temp working space
#ifndef SSI_TX_BUFFER_NUM_SAMPLES
#define SSI_TX_BUFFER_NUM_SAMPLES 128
#endif
int32_t spareRenderingBuffer[2][SSI_TX_BUFFER_NUM_SAMPLES] = {};

// Phase 8: SampleCluster destructor stub
// Real destructor references audioFileManager, Cluster::destroy() — too deep to compile.
#include "model/sample/sample_cluster.h"
SampleCluster::~SampleCluster() {}
