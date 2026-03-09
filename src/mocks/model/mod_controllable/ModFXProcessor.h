// Shadow header: adds forward declaration of UnpatchedParamSet before the real
// ModFXProcessor.h uses it as a pointer parameter type.
#pragma once
class UnpatchedParamSet;
#include_next "model/mod_controllable/ModFXProcessor.h"
