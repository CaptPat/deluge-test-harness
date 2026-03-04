#pragma once

// Mock timer system — advances simulated time for test scenarios.
// The actual timer function declarations come from RZA1/ostm/ostm.h.
// This header only adds the test-specific passMockTime function.
extern "C" {
void passMockTime(double seconds);
}
