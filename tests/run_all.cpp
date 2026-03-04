#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/MemoryLeakWarningPlugin.h"

int main(int argc, char** argv) {
	// Disable leak detection — our mock layer uses std::vector with inline
	// storage that CppUTest misidentifies as leaks.
	MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
	return CommandLineTestRunner::RunAllTests(argc, argv);
}
