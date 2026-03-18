// Integration tests for Source packed filenames and dirPath deduplication.
//
// Tests exercise Source::getFilename(), getFullPath(), isPackedMode(),
// unpackFilenames(), and revertToFullPaths() with real PackedFilenames
// and String objects against multisample ranges.

#include "CppUTest/TestHarness.h"
#include "processing/source.h"
#include "storage/audio/audio_file_holder.h"
#include "storage/multi_range/multi_range.h"
#include "storage/multi_range/multi_range_array.h"
#include "util/d_string.h"
#include "util/path_utils.h"

namespace {

// Helper to insert N multisample ranges into a Source and return the source.
// Each range covers a 1-note zone: topNote = rangeIndex.
void insertRanges(Source& src, int32_t count) {
	for (int32_t i = 0; i < count; i++) {
		MultiRange* r = src.ranges.insertMultiRange(i);
		CHECK(r != nullptr);
		r->topNote = static_cast<int16_t>(i);
	}
	CHECK_EQUAL(count, src.ranges.getNumElements());
}

// Pack N filenames into source with a shared directory prefix.
void packPianoNames(Source& src, int32_t count, const char* dir) {
	src.dirPath.set(dir);
	Error err = src.packedNames.beginPacking(count, count * 12);
	CHECK(err == Error::NONE);

	char name[32];
	for (int32_t i = 0; i < count; i++) {
		snprintf(name, sizeof(name), "note_%03d.wav", i);
		err = src.packedNames.addName(name);
		CHECK(err == Error::NONE);
	}
	src.packedNames.finishPacking();
}

} // namespace

TEST_GROUP(SourcePackedFilenames){};

// ── isPackedMode ────────────────────────────────────────────────────────

TEST(SourcePackedFilenames, DefaultIsNotPacked) {
	Source src;
	CHECK_FALSE(src.isPackedMode());
}

TEST(SourcePackedFilenames, PackedModeAfterPacking) {
	Source src;
	insertRanges(src, 4);
	packPianoNames(src, 4, "SAMPLES/PIANOS/");
	CHECK(src.isPackedMode());
}

// ── getFilename — packed mode ───────────────────────────────────────────

TEST(SourcePackedFilenames, GetFilenameFromPackedBuffer) {
	Source src;
	insertRanges(src, 3);
	packPianoNames(src, 3, "SAMPLES/PIANOS/");

	STRCMP_EQUAL("note_000.wav", src.getFilename(0));
	STRCMP_EQUAL("note_001.wav", src.getFilename(1));
	STRCMP_EQUAL("note_002.wav", src.getFilename(2));
}

TEST(SourcePackedFilenames, GetFilenameOutOfRangeReturnsEmpty) {
	Source src;
	insertRanges(src, 2);
	packPianoNames(src, 2, "SAMPLES/");

	STRCMP_EQUAL("", src.getFilename(-1));
	STRCMP_EQUAL("", src.getFilename(5));
}

// ── getFilename — unpacked mode (filePath on holders) ───────────────────

TEST(SourcePackedFilenames, GetFilenameFromHolderFilePath) {
	Source src;
	insertRanges(src, 2);

	src.ranges.getElement(0)->getAudioFileHolder()->filePath.set("kick.wav");
	src.ranges.getElement(1)->getAudioFileHolder()->filePath.set("snare.wav");

	CHECK_FALSE(src.isPackedMode());
	STRCMP_EQUAL("kick.wav", src.getFilename(0));
	STRCMP_EQUAL("snare.wav", src.getFilename(1));
}

// ── getFullPath — packed mode with dirPath ──────────────────────────────

TEST(SourcePackedFilenames, GetFullPathReconstructsDirPlusName) {
	Source src;
	insertRanges(src, 2);
	packPianoNames(src, 2, "SAMPLES/PIANOS/");

	String out;
	Error err = src.getFullPath(0, out);
	CHECK(err == Error::NONE);
	STRCMP_EQUAL("SAMPLES/PIANOS/note_000.wav", out.get());

	err = src.getFullPath(1, out);
	CHECK(err == Error::NONE);
	STRCMP_EQUAL("SAMPLES/PIANOS/note_001.wav", out.get());
}

TEST(SourcePackedFilenames, GetFullPathEmptyForOutOfRange) {
	Source src;
	insertRanges(src, 1);
	packPianoNames(src, 1, "SAMPLES/");

	String out;
	out.set("garbage");
	Error err = src.getFullPath(5, out);
	CHECK(err == Error::NONE);
	CHECK(out.isEmpty());
}

// ── getFullPath — unpacked mode with dirPath ────────────────────────────

TEST(SourcePackedFilenames, GetFullPathUnpackedWithDirPath) {
	Source src;
	insertRanges(src, 1);
	src.dirPath.set("SAMPLES/DRUMS/");
	src.ranges.getElement(0)->getAudioFileHolder()->filePath.set("kick.wav");

	String out;
	Error err = src.getFullPath(0, out);
	CHECK(err == Error::NONE);
	STRCMP_EQUAL("SAMPLES/DRUMS/kick.wav", out.get());
}

// ── getFullPath — no dirPath ────────────────────────────────────────────

TEST(SourcePackedFilenames, GetFullPathNoDirReturnsFilename) {
	Source src;
	insertRanges(src, 1);
	src.ranges.getElement(0)->getAudioFileHolder()->filePath.set("SAMPLES/PIANOS/C3.wav");

	String out;
	Error err = src.getFullPath(0, out);
	CHECK(err == Error::NONE);
	STRCMP_EQUAL("SAMPLES/PIANOS/C3.wav", out.get());
}

// ── unpackFilenames ─────────────────────────────────────────────────────

TEST(SourcePackedFilenames, UnpackMovesNamesToHolders) {
	Source src;
	insertRanges(src, 3);
	packPianoNames(src, 3, "SAMPLES/PIANOS/");

	CHECK(src.isPackedMode());
	src.unpackFilenames();
	CHECK_FALSE(src.isPackedMode());

	// Names should now be on holders (filename only, no dir)
	STRCMP_EQUAL("note_000.wav", src.ranges.getElement(0)->getAudioFileHolder()->filePath.get());
	STRCMP_EQUAL("note_001.wav", src.ranges.getElement(1)->getAudioFileHolder()->filePath.get());
	STRCMP_EQUAL("note_002.wav", src.ranges.getElement(2)->getAudioFileHolder()->filePath.get());

	// dirPath preserved (unpackFilenames doesn't clear it)
	STRCMP_EQUAL("SAMPLES/PIANOS/", src.dirPath.get());
}

TEST(SourcePackedFilenames, UnpackWhenAlreadyUnpackedIsNoop) {
	Source src;
	insertRanges(src, 1);
	src.ranges.getElement(0)->getAudioFileHolder()->filePath.set("test.wav");

	src.unpackFilenames(); // Should not crash or change anything
	STRCMP_EQUAL("test.wav", src.ranges.getElement(0)->getAudioFileHolder()->filePath.get());
}

// ── revertToFullPaths — packed mode ─────────────────────────────────────

TEST(SourcePackedFilenames, RevertPackedReconstructsFullPaths) {
	Source src;
	insertRanges(src, 3);
	packPianoNames(src, 3, "SAMPLES/PIANOS/");

	src.revertToFullPaths();

	CHECK_FALSE(src.isPackedMode());
	CHECK(src.dirPath.isEmpty());

	STRCMP_EQUAL("SAMPLES/PIANOS/note_000.wav", src.ranges.getElement(0)->getAudioFileHolder()->filePath.get());
	STRCMP_EQUAL("SAMPLES/PIANOS/note_001.wav", src.ranges.getElement(1)->getAudioFileHolder()->filePath.get());
	STRCMP_EQUAL("SAMPLES/PIANOS/note_002.wav", src.ranges.getElement(2)->getAudioFileHolder()->filePath.get());
}

// ── revertToFullPaths — unpacked mode with dirPath ──────────────────────

TEST(SourcePackedFilenames, RevertUnpackedWithDirReconstructsFullPaths) {
	Source src;
	insertRanges(src, 2);
	src.dirPath.set("SAMPLES/DRUMS/");
	src.ranges.getElement(0)->getAudioFileHolder()->filePath.set("kick.wav");
	src.ranges.getElement(1)->getAudioFileHolder()->filePath.set("snare.wav");

	src.revertToFullPaths();

	CHECK(src.dirPath.isEmpty());
	STRCMP_EQUAL("SAMPLES/DRUMS/kick.wav", src.ranges.getElement(0)->getAudioFileHolder()->filePath.get());
	STRCMP_EQUAL("SAMPLES/DRUMS/snare.wav", src.ranges.getElement(1)->getAudioFileHolder()->filePath.get());
}

// ── revertToFullPaths — no dirPath, no packed ───────────────────────────

TEST(SourcePackedFilenames, RevertNoDirNoPackedIsNoop) {
	Source src;
	insertRanges(src, 1);
	src.ranges.getElement(0)->getAudioFileHolder()->filePath.set("SAMPLES/DRUMS/kick.wav");

	src.revertToFullPaths();

	// Should be unchanged
	STRCMP_EQUAL("SAMPLES/DRUMS/kick.wav", src.ranges.getElement(0)->getAudioFileHolder()->filePath.get());
}

// ── Piano scenario (88 keys) ───────────────────────────────────────────

TEST(SourcePackedFilenames, PianoScenario88Keys) {
	Source src;
	insertRanges(src, 88);
	packPianoNames(src, 88, "SAMPLES/PIANOS/STEINWAY/");

	CHECK(src.isPackedMode());
	CHECK_EQUAL(88, src.packedNames.getCount());

	// Spot-check first, middle, last
	String out;
	src.getFullPath(0, out);
	STRCMP_EQUAL("SAMPLES/PIANOS/STEINWAY/note_000.wav", out.get());

	src.getFullPath(43, out);
	STRCMP_EQUAL("SAMPLES/PIANOS/STEINWAY/note_043.wav", out.get());

	src.getFullPath(87, out);
	STRCMP_EQUAL("SAMPLES/PIANOS/STEINWAY/note_087.wav", out.get());

	// Revert to full paths
	src.revertToFullPaths();
	CHECK_FALSE(src.isPackedMode());
	STRCMP_EQUAL("SAMPLES/PIANOS/STEINWAY/note_000.wav",
	            src.ranges.getElement(0)->getAudioFileHolder()->filePath.get());
	STRCMP_EQUAL("SAMPLES/PIANOS/STEINWAY/note_087.wav",
	            src.ranges.getElement(87)->getAudioFileHolder()->filePath.get());
}

// ── Packed with no dirPath ──────────────────────────────────────────────

TEST(SourcePackedFilenames, PackedWithNoDirPathUsesNameAsFullPath) {
	Source src;
	insertRanges(src, 2);

	// Pack with full paths as names, no dirPath
	Error err = src.packedNames.beginPacking(2, 128);
	CHECK(err == Error::NONE);
	src.packedNames.addName("SAMPLES/A/note_000.wav");
	src.packedNames.addName("SAMPLES/B/note_001.wav");
	src.packedNames.finishPacking();

	String out;
	src.getFullPath(0, out);
	STRCMP_EQUAL("SAMPLES/A/note_000.wav", out.get());

	src.getFullPath(1, out);
	STRCMP_EQUAL("SAMPLES/B/note_001.wav", out.get());
}

// ── Path utilities (splitPath + dirMatches) ─────────────────────────────

TEST_GROUP(PathUtils){};

TEST(PathUtils, SplitPathNested) {
	const char* dir;
	int32_t dirLen;
	const char* name;
	splitPath("SAMPLES/PIANOS/STEINWAY/C3.WAV", &dir, &dirLen, &name);

	CHECK_EQUAL(24, dirLen);
	STRCMP_EQUAL("C3.WAV", name);
}

TEST(PathUtils, SplitPathSingleDir) {
	const char* dir;
	int32_t dirLen;
	const char* name;
	splitPath("SAMPLES/kick.wav", &dir, &dirLen, &name);

	CHECK_EQUAL(8, dirLen);
	STRCMP_EQUAL("kick.wav", name);
}

TEST(PathUtils, SplitPathNoDir) {
	const char* dir;
	int32_t dirLen;
	const char* name;
	splitPath("kick.wav", &dir, &dirLen, &name);

	CHECK_EQUAL(0, dirLen);
	STRCMP_EQUAL("kick.wav", name);
}

TEST(PathUtils, DirMatchesPositive) {
	CHECK(dirMatches("SAMPLES/PIANOS/", 15, "SAMPLES/PIANOS/C3.WAV"));
}

TEST(PathUtils, DirMatchesNegative) {
	CHECK_FALSE(dirMatches("SAMPLES/PIANOS/", 15, "SAMPLES/DRUMS/kick.wav"));
}

TEST(PathUtils, DirMatchesZeroLength) {
	CHECK_FALSE(dirMatches("", 0, "kick.wav"));
}
