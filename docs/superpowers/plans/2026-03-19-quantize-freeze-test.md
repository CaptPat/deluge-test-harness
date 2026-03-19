# Quantize Freeze Guard Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a source contract test verifying the UI_MODE_QUANTIZE handler in AutomationView, and update harness tracking to include the new branch.

**Architecture:** Source contract test (reads firmware .cpp/.h files, asserts code patterns exist) following the `sd_race_guard_test.cpp` pattern. No new mocks needed.

**Tech Stack:** CppUTest, C++17 filesystem, string search on firmware source files

---

### Task 1: Update firmware submodule to current nightly

**Files:**
- Modify: `firmware` (git submodule)

- [ ] **Step 1: Update submodule**

```bash
cd p:/VSCodeDesktop/projects/deluge-test-harness
git -C firmware fetch origin
git -C firmware checkout f983ff89
git add firmware
```

- [ ] **Step 2: Verify submodule is at correct commit**

Run: `git -C firmware log --oneline -1`
Expected: `f983ff89 Merge branch 'fix/velocity-view-quantize-freeze' into personal/nightly`

---

### Task 2: Write the source contract test

**Files:**
- Create: `tests/meta/quantize_freeze_guard_test.cpp`

- [ ] **Step 1: Write the test file**

```cpp
// Source contract test: verify that AutomationView handles UI_MODE_QUANTIZE
// in handleAuditionPadAction() to prevent partial freeze when quantizing
// notes in Velocity View.
//
// Background (upstream #3718): Quantizing notes in Velocity View caused a
// partial freeze because AutomationView::handleAuditionPadAction() had no
// handler for UI_MODE_QUANTIZE. The pad release went through the generic
// audition path which never called commandStopQuantize(), leaving the UI
// mode permanently active and blocking most inputs.
//
// The fix (fix/velocity-view-quantize-freeze):
//   1. Add UI_MODE_QUANTIZE check to AutomationView::handleAuditionPadAction()
//   2. Move commandStopQuantize to public visibility

#include "CppUTest/TestHarness.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path firmwareRoot() {
	fs::path p = fs::current_path();
	while (!p.empty() && !fs::exists(p / "firmware" / "src" / "deluge")) {
		auto parent = p.parent_path();
		if (parent == p)
			break;
		p = parent;
	}
	return p / "firmware";
}

std::string readFile(const fs::path& path) {
	std::ifstream f(path);
	if (!f.is_open())
		return "";
	return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

TEST_GROUP(QuantizeFreezeGuard){};

// AutomationView::handleAuditionPadAction must check UI_MODE_QUANTIZE
// and call commandStopQuantize on pad release, before the generic
// auditionPadActionUIModes branch handles it silently.
TEST(QuantizeFreezeGuard, automationViewHandlesQuantizeMode) {
	fs::path cppPath = firmwareRoot() / "src" / "deluge" / "gui" / "views" / "automation_view.cpp";
	CHECK_TEXT(fs::exists(cppPath), "automation_view.cpp not found — is firmware submodule checked out?");

	std::string src = readFile(cppPath);
	CHECK_TEXT(!src.empty(), "Failed to read automation_view.cpp");

	// Find handleAuditionPadAction
	auto funcPos = src.find("AutomationView::handleAuditionPadAction");
	CHECK_TEXT(funcPos != std::string::npos,
	           "AutomationView::handleAuditionPadAction not found in automation_view.cpp");

	// Find the generic auditionPadActionUIModes branch (this is the fallthrough we must beat)
	auto genericPos = src.find("auditionPadActionUIModes", funcPos);
	CHECK_TEXT(genericPos != std::string::npos,
	           "auditionPadActionUIModes branch not found in handleAuditionPadAction");

	// UI_MODE_QUANTIZE must appear BEFORE the generic branch
	auto quantizePos = src.find("UI_MODE_QUANTIZE", funcPos);
	CHECK_TEXT(quantizePos != std::string::npos && quantizePos < genericPos,
	           "UI_MODE_QUANTIZE handler must appear in handleAuditionPadAction before "
	           "the generic auditionPadActionUIModes branch (fix/velocity-view-quantize-freeze, "
	           "upstream #3718)");

	// commandStopQuantize must be called in the handler
	auto stopPos = src.find("commandStopQuantize", funcPos);
	CHECK_TEXT(stopPos != std::string::npos && stopPos < genericPos,
	           "commandStopQuantize must be called in the UI_MODE_QUANTIZE handler "
	           "to properly exit quantize mode on pad release");
}

// commandStopQuantize must be public so AutomationView can call it.
TEST(QuantizeFreezeGuard, commandStopQuantizeIsPublic) {
	fs::path hPath = firmwareRoot() / "src" / "deluge" / "gui" / "views" / "instrument_clip_view.h";
	CHECK_TEXT(fs::exists(hPath), "instrument_clip_view.h not found");

	std::string src = readFile(hPath);
	CHECK_TEXT(!src.empty(), "Failed to read instrument_clip_view.h");

	auto publicPos = src.find("public:");
	auto privatePos = src.find("private:", publicPos);
	CHECK_TEXT(publicPos != std::string::npos && privatePos != std::string::npos,
	           "Could not find public/private sections in instrument_clip_view.h");

	auto methodPos = src.find("commandStopQuantize", publicPos);
	CHECK_TEXT(methodPos != std::string::npos && methodPos < privatePos,
	           "commandStopQuantize must be in the public section of InstrumentClipView "
	           "so AutomationView can call it (fix/velocity-view-quantize-freeze)");
}

} // namespace
```

- [ ] **Step 2: Register test in CMakeLists.txt**

Add after line 469 (`tests/meta/sd_race_guard_test.cpp`):

```cmake
    # Meta: source contract — quantize freeze guard (upstream #3718)
    tests/meta/quantize_freeze_guard_test.cpp
```

---

### Task 3: Build and run tests

- [ ] **Step 1: Build**

```bash
cd p:/VSCodeDesktop/projects/deluge-test-harness
cmake -B build -G Ninja && cmake --build build
```

- [ ] **Step 2: Run tests**

```bash
ctest --test-dir build --output-on-failure
```

Expected: All tests pass including new `QuantizeFreezeGuard` tests.

- [ ] **Step 3: Verify new tests appear**

```bash
./build/HarnessTests -g QuantizeFreezeGuard -v
```

Expected: 2 tests run, 2 pass.

---

### Task 4: Update branch_status.md

**Files:**
- Modify: `branch_status.md`

- [ ] **Step 1: Add branch to bugfix table**

Add row to the bugfix branches table (after `bugfix/wavetable-mod-knob-overwrite`):

```markdown
| `fix/velocity-view-quantize-freeze`         | active     | has-test    | Missing `UI_MODE_QUANTIZE` handler in AutomationView causes partial freeze when quantizing in Velocity View | `tests/meta/quantize_freeze_guard_test.cpp` (source contract: quantize handler + public visibility) | #3718 (open) |
```

- [ ] **Step 2: Add test quality entry**

Add row to the test quality table:

```markdown
| `fix/velocity-view-quantize-freeze`     | **Good** -- source contract test verifies `UI_MODE_QUANTIZE` handler in `handleAuditionPadAction` and public `commandStopQuantize` |
```

- [ ] **Step 3: Verify "Remaining untested testable branches" still says "None"**

It should still say "None" since we just added a test.

---

### Task 5: Commit and push

- [ ] **Step 1: Stage all changes**

```bash
git add firmware tests/meta/quantize_freeze_guard_test.cpp CMakeLists.txt branch_status.md
```

- [ ] **Step 2: Commit**

```bash
git commit -m "test: add quantize freeze guard test + update submodule

Add source contract test verifying UI_MODE_QUANTIZE handler in
AutomationView::handleAuditionPadAction and public visibility of
commandStopQuantize (upstream #3718, fix/velocity-view-quantize-freeze).

Update firmware submodule to nightly f983ff89."
```

- [ ] **Step 3: Push**

```bash
git push origin main
```
