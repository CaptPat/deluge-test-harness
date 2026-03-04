// Shadow header replacing firmware's lib/printf.h
// The real header provides snprintf_/vsnprintf_ custom implementations.
// We redirect to standard C library functions.

#pragma once
#include <cstdio>

#define snprintf_ snprintf
#define vsnprintf_ vsnprintf
