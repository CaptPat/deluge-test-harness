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

// Default readInput: returns 1 (idle state for encoders).
// Tests can override with a strong symbol for controllable GPIO mocking.
__attribute__((weak)) uint16_t readInput(uint8_t p, uint8_t q) { (void)p; (void)q; return 1; }

} // extern "C"

// l10n: ensure chosenLanguage is never null — tests that call l10n::get()
// (e.g. getPatchedParamDisplayName) crash if this isn't set before they run.
#include "gui/l10n/l10n.h"
#include "gui/l10n/strings.h"
static struct L10nInit {
	L10nInit() { deluge::l10n::chosenLanguage = &deluge::l10n::built_in::english; }
} l10nInit;

// Global buffers referenced by firmware utility code
char miscStringBuffer[256] = {};

// Phase 12: spareRenderingBuffer — used by delay.cpp as temp working space
#ifndef SSI_TX_BUFFER_NUM_SAMPLES
#define SSI_TX_BUFFER_NUM_SAMPLES 128
#endif
int32_t spareRenderingBuffer[2][SSI_TX_BUFFER_NUM_SAMPLES] = {};

// Phase 16: PerformanceView global for consequence_performance_view_press
#include "gui/views/performance_view.h"
PerformanceView performanceView{};

// Phase H2: View global for param_manager.cpp
#include "gui/views/view.h"
View view{};

// Phase I: ActionLogger global for param_set.cpp
#include "model/action/action_logger.h"
ActionLogger actionLogger;

// Phase 16: Session global and defaultClipSectionColours for clip_instance.cpp
#include "playback/mode/session.h"
Session session;
const RGB defaultClipSectionColours[] = {
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
    RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128), RGB::monochrome(128),
};

// Phase 8: SampleCluster destructor stub
// Real destructor references audioFileManager, Cluster::destroy() — too deep to compile.
#include "model/sample/sample_cluster.h"
SampleCluster::~SampleCluster() {}
