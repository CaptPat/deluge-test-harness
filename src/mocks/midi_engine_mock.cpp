#include "midi_engine_mock.h"

namespace MockMIDIEngine {

void reset() {
	sentMessages.clear();
	pendingInbound.clear();
}

void sendMessage(uint8_t status, uint8_t data1, uint8_t data2) {
	sentMessages.push_back({status, data1, data2});
}

void injectInbound(uint8_t status, uint8_t data1, uint8_t data2) {
	pendingInbound.push_back({status, data1, data2});
}

bool hasSentMessages() { return !sentMessages.empty(); }

MockMIDIMessage getLastSent() { return sentMessages.back(); }

} // namespace MockMIDIEngine
