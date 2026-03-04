#pragma once

#include <cstdint>
#include <vector>

// Mock MIDI engine — captures outbound messages and allows injecting inbound ones.
struct MockMIDIMessage {
	uint8_t status;
	uint8_t data1;
	uint8_t data2;
};

namespace MockMIDIEngine {

inline std::vector<MockMIDIMessage> sentMessages;
inline std::vector<MockMIDIMessage> pendingInbound;

void reset();
void sendMessage(uint8_t status, uint8_t data1, uint8_t data2);
void injectInbound(uint8_t status, uint8_t data1, uint8_t data2);
bool hasSentMessages();
MockMIDIMessage getLastSent();

} // namespace MockMIDIEngine
