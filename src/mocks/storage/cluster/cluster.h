// Shadow header replacing firmware's storage/cluster/cluster.h for x86.
// The real header defines audio cluster types; not needed for container tests.

#pragma once

#include <cstdint>

class Cluster {};
