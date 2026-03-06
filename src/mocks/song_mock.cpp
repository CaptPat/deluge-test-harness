#include "song_mock.h"
#include "model/output.h"

Song testSong;
Song* currentSong = &testSong;
Song* preLoadedSong = nullptr;

void Song::addOutput(Output* output, bool atStart) {
	(void)atStart;
	output->next = firstOutput;
	firstOutput = output;
}

int32_t Song::removeOutputFromMainList(Output* output) {
	if (!firstOutput)
		return -1;
	if (firstOutput == output) {
		firstOutput = output->next;
		output->next = nullptr;
		return 0;
	}
	int32_t index = 0;
	for (Output* o = firstOutput; o; o = o->next) {
		index++;
		if (o->next == output) {
			o->next = output->next;
			output->next = nullptr;
			return index;
		}
	}
	return -1;
}
