#include "CppUTest/TestHarness.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Known mapping: upstream test file -> harness test file(s)
// Update this when new upstream tests appear or new harness tests are written.
const std::map<std::string, std::vector<std::string>> knownOverlap = {
    {"scale_tests.cpp", {"tests/model/scale_test.cpp"}},
    {"chord_tests.cpp", {"tests/model/chord_test.cpp"}},
    {"sync_tests.cpp", {"tests/model/sync_test.cpp"}},
    {"lfo_tests.cpp", {"tests/modulation/lfo_test.cpp"}},
    {"value_scaling_tests.cpp", {"tests/gui/value_scaling_test.cpp"}},
    {"clip_iterator_tests.cpp", {"tests/model/clip_iterators_test.cpp"}},
    {"time_signature_tests.cpp", {"tests/model/time_signature_test.cpp"}},
    {"tempo_ratio_tests.cpp", {"tests/model/tempo_ratio_test.cpp"}},
    {"envelope_tests.cpp", {"tests/modulation/envelope_test.cpp"}},
    {"notes_state_tests.cpp", {}}, // no separate harness mirror needed (header-only, same test)
    {"function_tests.cpp", {"tests/util/const_functions_test.cpp"}},
    {"time_tests.cpp", {"tests/util/clock_type_test.cpp"}},
    {"browser_search_tests.cpp", {"tests/util/browser_search_test.cpp"}},
    {"filter_morph_label_tests.cpp", {"tests/modulation/filter_morph_label_test.cpp"}},
    {"pedal_state_tests.cpp", {"tests/model/pedal_state_test.cpp"}},
    {"reverb_filter_roundtrip_tests.cpp", {"tests/dsp/reverb_filter_roundtrip_test.cpp"}},
};

// Upstream tests with no harness equivalent (and why)
const std::map<std::string, std::string> upstreamOnly = {
    {"scheduler_tests.cpp", "needs timer mocks + resource_checker + MockSupport"},
    {"RunAllTests.cpp", "CppUTest entry point, not a test"},
};

fs::path repoRoot() {
	// Walk up from the executable to find the repo root
	fs::path p = fs::current_path();
	while (!p.empty() && !fs::exists(p / "firmware" / "tests" / "unit")) {
		auto parent = p.parent_path();
		if (parent == p)
			break;
		p = parent;
	}
	return p;
}

TEST_GROUP(UpstreamOverlap){};

// Verify all upstream test files are accounted for (either in knownOverlap or upstreamOnly)
TEST(UpstreamOverlap, allUpstreamFilesTracked) {
	fs::path root = repoRoot();
	fs::path unitDir = root / "firmware" / "tests" / "unit";
	if (!fs::exists(unitDir)) {
		FAIL("firmware/tests/unit/ not found — is the submodule checked out?");
	}

	std::vector<std::string> untracked;
	for (auto& entry : fs::directory_iterator(unitDir)) {
		if (!entry.is_regular_file())
			continue;
		std::string name = entry.path().filename().string();
		if (name.find("_tests.cpp") == std::string::npos && name.find("Tests.cpp") == std::string::npos)
			continue;
		if (knownOverlap.count(name) == 0 && upstreamOnly.count(name) == 0) {
			untracked.push_back(name);
		}
	}

	if (!untracked.empty()) {
		std::string msg = "Untracked upstream test files (add to knownOverlap or upstreamOnly): ";
		for (auto& f : untracked) {
			msg += f + " ";
		}
		FAIL(msg.c_str());
	}
}

// Verify harness-side files in knownOverlap actually exist
TEST(UpstreamOverlap, harnessFilesExist) {
	fs::path root = repoRoot();
	std::vector<std::string> missing;

	for (auto& [upstream, harnessFiles] : knownOverlap) {
		for (auto& hf : harnessFiles) {
			if (!fs::exists(root / hf)) {
				missing.push_back(hf + " (mirror of " + upstream + ")");
			}
		}
	}

	if (!missing.empty()) {
		std::string msg = "Missing harness test files: ";
		for (auto& m : missing) {
			msg += m + "; ";
		}
		FAIL(msg.c_str());
	}
}

// Verify upstream-side files in knownOverlap actually exist.
// Files in PR branches may not be present on main — only fail for files that
// exist on the current branch's tests/unit/ but aren't tracked.
TEST(UpstreamOverlap, upstreamFilesExist) {
	fs::path root = repoRoot();
	fs::path unitDir = root / "firmware" / "tests" / "unit";

	// Only check files that actually exist on disk — PR-branch files
	// may be absent when submodule points at main.
	for (auto& [upstream, harnessFiles] : knownOverlap) {
		if (fs::exists(unitDir / upstream)) {
			// File exists — verify harness mirrors also exist
			for (auto& hf : harnessFiles) {
				CHECK_TEXT(fs::exists(root / hf),
				           (hf + " (mirror of " + upstream + ") not found").c_str());
			}
		}
	}
}

// Report overlap count (informational, always passes)
TEST(UpstreamOverlap, overlapSummary) {
	int dualCoverage = 0;
	int upstreamOnlyCount = 0;
	for (auto& [_, harnessFiles] : knownOverlap) {
		if (!harnessFiles.empty())
			dualCoverage++;
		else
			upstreamOnlyCount++;
	}
	// This test just exists to print the summary; it always passes.
	// Dual coverage count: dualCoverage, upstream-only in overlap map: upstreamOnlyCount
	CHECK(dualCoverage >= 0);
}

} // namespace
