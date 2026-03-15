// Shadow header replacing firmware's model/sample/sample_recorder.h
// The real header includes fatfs/fatfs.hpp which is unavailable on x86.
// Sound only uses SampleRecorder* as a pointer parameter, so a forward declaration suffices.
#pragma once

class SampleRecorder;
