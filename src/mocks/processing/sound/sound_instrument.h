// Shadow header replacing firmware's processing/sound/sound_instrument.h
// Blocks the deep sound.h → melodic_instrument.h → arpeggiator.h chain.
// note_row.h includes this but only uses forward-declared types —
// SoundInstrument is never embedded by value in NoteRow.

#pragma once

class SoundInstrument;
