// AutoParam stubs for the test harness.
// auto_param.cpp (2911 lines) cannot be compiled — it depends on GUI, playback,
// action logger, display, etc.  Instead, we provide real implementations for the
// data-structure methods (they only touch ParamNodeVector, which is already
// compiled) and no-op stubs for everything else.

#include "modulation/automation/auto_param.h"
#include "modulation/params/param_node.h"
#include "util/fixedpoint.h"
#include <cstring>

// ── Real implementations (ported from auto_param.cpp) ────────────────────

AutoParam::AutoParam() {
	init();
	currentValue = 0;
	valueIncrementPerHalfTick = 0;
	renewedOverridingAtTime = 0;
}

void AutoParam::init() {
	nodes.init();
}

void AutoParam::cloneFrom(AutoParam* otherParam, bool copyAutomation) {
	if (copyAutomation) {
		nodes.cloneFrom(&otherParam->nodes);
	}
	else {
		nodes.init();
	}
	currentValue = otherParam->currentValue;
	renewedOverridingAtTime = 0;
}

void AutoParam::copyOverridingFrom(AutoParam* otherParam) {
	if (otherParam->renewedOverridingAtTime) {
		renewedOverridingAtTime = otherParam->renewedOverridingAtTime;
		valueIncrementPerHalfTick = 0;
	}
	currentValue = otherParam->currentValue;
}

Error AutoParam::beenCloned(bool copyAutomation, int32_t reverseDirectionWithLength) {
	Error error = Error::NONE;
	if (copyAutomation) {
		if (reverseDirectionWithLength && nodes.getNumElements()) {
			ParamNodeVector oldNodes = nodes;
			int32_t numNodes = nodes.getNumElements();
			nodes.init();
			error = nodes.insertAtIndex(0, numNodes);
			if (error == Error::NONE) {
				ParamNode* rightmostNode = (ParamNode*)oldNodes.getElementAddress(numNodes - 1);
				int32_t oldNodeToLeftValue = rightmostNode->value;
				ParamNode* leftmostNode = (ParamNode*)oldNodes.getElementAddress(0);
				bool anythingAtZero = !leftmostNode->pos;
				for (int32_t iOld = 0; iOld < numNodes; iOld++) {
					int32_t iNew = -iOld - !anythingAtZero;
					if (iNew < 0) {
						iNew += numNodes;
					}
					ParamNode* oldNode = (ParamNode*)oldNodes.getElementAddress(iOld);
					ParamNode* newNode = (ParamNode*)nodes.getElementAddress(iNew);
					int32_t iOldToRight = iOld + 1;
					if (iOldToRight == numNodes) {
						iOldToRight = 0;
					}
					ParamNode* oldNodeToRight = (ParamNode*)oldNodes.getElementAddress(iOldToRight);
					int32_t newPos = -oldNode->pos;
					if (newPos < 0) {
						newPos += reverseDirectionWithLength;
					}
					int32_t newValue = oldNode->interpolated ? oldNode->value : oldNodeToLeftValue;
					newNode->pos = newPos;
					newNode->value = newValue;
					newNode->interpolated = oldNodeToRight->interpolated;
					oldNodeToLeftValue = oldNode->value;
				}
			}
			oldNodes.init();
		}
		else {
			error = nodes.beenCloned();
		}
	}
	else {
		nodes.init();
	}
	renewedOverridingAtTime = 0;
	return error;
}

bool AutoParam::containsSomething(uint32_t neutralValue) {
	if (isAutomated()) {
		return true;
	}
	uint32_t* a = (uint32_t*)&currentValue;
	return (*a != (uint32_t)neutralValue);
}

bool AutoParam::containedSomethingBefore(bool wasAutomatedBefore, uint32_t valueBefore, uint32_t neutralValue) {
	return (wasAutomatedBefore || valueBefore != neutralValue);
}

void AutoParam::shiftValues(int32_t offset) {
	int64_t newValue = (int64_t)currentValue + offset;
	if (newValue >= (int64_t)2147483648u) {
		currentValue = 2147483647;
	}
	else if (newValue < (int64_t)2147483648u * -1) {
		currentValue = -2147483648;
	}
	else {
		currentValue = newValue;
	}

	for (int32_t i = 0; i < nodes.getNumElements(); i++) {
		ParamNode* thisNode = nodes.getElement(i);
		int64_t nv = (int64_t)thisNode->value + offset;
		if (nv >= (int64_t)2147483648u) {
			thisNode->value = 2147483647;
		}
		else if (nv < (int64_t)2147483648u * -1) {
			thisNode->value = -2147483648;
		}
		else {
			thisNode->value = nv;
		}
	}
}

int32_t AutoParam::setNodeAtPos(int32_t pos, int32_t value, bool shouldInterpolate) {
	int32_t i = nodes.search(pos, GREATER_OR_EQUAL);
	ParamNode* ourNode;

	if (i < nodes.getNumElements()) {
		ourNode = nodes.getElement(i);
		if (ourNode->pos == pos) {
			goto setupNode;
		}
	}

	{
		Error error = nodes.insertAtIndex(i);
		if (error != Error::NONE) {
			return -1;
		}
	}
	ourNode = nodes.getElement(i);
	ourNode->pos = pos;
setupNode:
	ourNode->value = value;
	ourNode->interpolated = shouldInterpolate;
	return i;
}

bool AutoParam::tickSamples(int32_t numSamples) {
	if (!valueIncrementPerHalfTick) {
		return false;
	}
	// Simplified: no playbackHandler dependency.  Just apply increment directly.
	int32_t oldValue = currentValue;
	currentValue += valueIncrementPerHalfTick * numSamples;
	bool overflowOccurred = (valueIncrementPerHalfTick >= 0) ? (currentValue < oldValue) : (currentValue > oldValue);
	if (overflowOccurred) {
		currentValue = (valueIncrementPerHalfTick >= 0) ? 2147483647 : -2147483648;
		valueIncrementPerHalfTick = 0;
	}
	return true;
}

bool AutoParam::tickTicks(int32_t numTicks) {
	if (valueIncrementPerHalfTick == 0) {
		return false;
	}
	currentValue = add_saturate(currentValue, valueIncrementPerHalfTick * numTicks * 2);
	return true;
}

void AutoParam::notifyPingpongOccurred() {
	valueIncrementPerHalfTick = -valueIncrementPerHalfTick;
}

void AutoParam::deleteAutomationBasicForSetup() {
	nodes.empty();
	valueIncrementPerHalfTick = 0;
	renewedOverridingAtTime = 0;
}

void AutoParam::insertTime(int32_t pos, int32_t lengthToInsert) {
	int32_t start = nodes.search(pos, GREATER_OR_EQUAL);
	for (int32_t i = start; i < nodes.getNumElements(); i++) {
		ParamNode* node = nodes.getElement(i);
		node->pos += lengthToInsert;
	}
}

void AutoParam::generateRepeats(uint32_t oldLength, uint32_t newLength, bool shouldPingpong) {
	if (!nodes.getNumElements()) {
		return;
	}

	ParamNode* firstNode = nodes.getFirst();
	deleteNodesBeyondPos(oldLength + firstNode->pos);

	if (shouldPingpong) {
		ParamNode* nodeAfterWrap = (ParamNode*)nodes.getElementAddress(0);
		bool nothingAtZero = nodeAfterWrap->pos;

		if (nothingAtZero) {
			ParamNode* nodeBeforeWrap = (ParamNode*)nodes.getElementAddress(nodes.getNumElements() - 1);
			bool nodeAfterWrapIsInterpolated = nodeAfterWrap->interpolated;
			int32_t valueAtZero;
			if (nodeAfterWrapIsInterpolated) {
				int64_t valueDistance = (int64_t)nodeAfterWrap->value - (int64_t)nodeBeforeWrap->value;
				int32_t ticksSinceLeftNode = oldLength - nodeBeforeWrap->pos;
				int32_t ticksBetweenNodes = ticksSinceLeftNode + nodeAfterWrap->pos;
				valueAtZero = nodeBeforeWrap->value + (valueDistance * ticksSinceLeftNode / ticksBetweenNodes);
			}
			else {
				valueAtZero = nodeBeforeWrap->value;
			}
			Error error = nodes.insertAtIndex(0);
			if (error != Error::NONE) {
				return;
			}
			ParamNode* zeroNode = (ParamNode*)nodes.getElementAddress(0);
			zeroNode->pos = 0;
			zeroNode->value = valueAtZero;
			zeroNode->interpolated = nodeAfterWrapIsInterpolated;
			nothingAtZero = false;
		}

		int32_t numRepeats = (uint32_t)(newLength - 1) / oldLength + 1;
		int32_t numNodesBefore = nodes.getNumElements();
		int32_t numToInsert = (numRepeats - 1) * numNodesBefore;
		if (numToInsert) {
			Error error = nodes.insertAtIndex(numNodesBefore, numToInsert);
			if (error != Error::NONE) {
				return;
			}
		}
		int32_t highestNodeIndex = numNodesBefore - 1;
		for (int32_t r = 1; r < numRepeats; r++) {
			for (int32_t iNewWithinRepeat = 0; iNewWithinRepeat < numNodesBefore; iNewWithinRepeat++) {
				int32_t iOld = iNewWithinRepeat;
				if (r & 1) {
					iOld = -iOld - nothingAtZero;
					if (iOld < 0) {
						iOld += numNodesBefore;
					}
				}
				ParamNode* oldNode = (ParamNode*)nodes.getElementAddress(iOld);
				int32_t newPos = oldNode->pos;
				if (r & 1) {
					newPos = -newPos;
					if (newPos < 0) {
						newPos += oldLength;
					}
				}
				newPos += oldLength * r;
				if (newPos >= (int32_t)newLength) {
					break;
				}
				int32_t newValue = oldNode->value;
				bool newInterpolated = oldNode->interpolated;
				if (r & 1) {
					if (!oldNode->interpolated) {
						int32_t iOldToLeft = iOld - 1;
						if (iOldToLeft < 0) {
							iOldToLeft += numNodesBefore;
						}
						ParamNode* oldNodeToLeft = (ParamNode*)nodes.getElementAddress(iOldToLeft);
						newValue = oldNodeToLeft->value;
					}
					int32_t iOldToRight = iOld + 1;
					if (iOldToRight >= numNodesBefore) {
						iOldToRight = 0;
					}
					ParamNode* oldNodeToRight = (ParamNode*)nodes.getElementAddress(iOldToRight);
					newInterpolated = oldNodeToRight->interpolated;
				}
				int32_t iNew = iNewWithinRepeat + numNodesBefore * r;
				ParamNode* newNode = (ParamNode*)nodes.getElementAddress(iNew);
				newNode->pos = newPos;
				newNode->value = newValue;
				newNode->interpolated = newInterpolated;
				highestNodeIndex = iNew;
			}
		}
		int32_t newNumNodes = highestNodeIndex + 1;
		int32_t numToDelete = nodes.getNumElements() - newNumNodes;
		if (numToDelete) {
			nodes.deleteAtIndex(newNumNodes, numToDelete);
		}
	}
	else {
		nodes.generateRepeats(oldLength, newLength);
	}
}

void AutoParam::shiftHorizontally(int32_t amount, int32_t effectiveLength) {
	nodes.shiftHorizontal(amount, effectiveLength);
}

// ── No-op stubs (depend on GUI, Action, playback, etc.) ──────────────────

void AutoParam::setCurrentValueInResponseToUserInput(int32_t value, ModelStackWithAutoParam const*, bool, int32_t,
                                                     bool, bool) {
	currentValue = value;
}

int32_t AutoParam::processCurrentPos(ModelStackWithAutoParam const*, bool, bool, bool, bool) {
	return 0;
}

void AutoParam::setValueForRegion(uint32_t, uint32_t, int32_t value, ModelStackWithAutoParam const*, ActionType) {
	currentValue = value;
}

void AutoParam::setValuePossiblyForRegion(int32_t value, ModelStackWithAutoParam const*, int32_t, int32_t, bool) {
	currentValue = value;
}

int32_t AutoParam::getValueAtPos(uint32_t, ModelStackWithAutoParam const*, bool) {
	return currentValue;
}

void AutoParam::setPlayPos(uint32_t, ModelStackWithAutoParam const*, bool) {}

bool AutoParam::grabValueFromPos(uint32_t, ModelStackWithAutoParam const*) {
	return false;
}

void AutoParam::trimToLength(uint32_t, Action*, ModelStackWithAutoParam const*) {}

void AutoParam::deleteAutomation(Action*, ModelStackWithAutoParam const*, bool) {
	deleteAutomationBasicForSetup();
}

void AutoParam::writeToFile(Serializer&, bool, int32_t*) {}

Error AutoParam::readFromFile(Deserializer&, int32_t) {
	return Error::NONE;
}

void AutoParam::shiftParamVolumeByDB(float) {}

void AutoParam::swapState(AutoParamState*, ModelStackWithAutoParam const*) {}

void AutoParam::copy(int32_t, int32_t, CopiedParamAutomation*, bool, ModelStackWithAutoParam const*) {}

void AutoParam::paste(int32_t, int32_t, float, ModelStackWithAutoParam const*, CopiedParamAutomation*, bool) {}

Error AutoParam::makeInterpolationGoodAgain(int32_t, int32_t) {
	return Error::NONE;
}

void AutoParam::transposeCCValuesToChannelPressureValues() {}

void AutoParam::deleteTime(int32_t, int32_t, ModelStackWithAutoParam*) {}

void AutoParam::appendParam(AutoParam* otherParam, int32_t oldLength, int32_t, bool) {
	int32_t numToInsert = otherParam->nodes.getNumElements();
	if (!numToInsert) {
		return;
	}
	int32_t insertAt = nodes.getNumElements();
	Error error = nodes.insertAtIndex(insertAt, numToInsert);
	if (error != Error::NONE) {
		return;
	}
	for (int32_t i = 0; i < numToInsert; i++) {
		ParamNode* src = otherParam->nodes.getElement(i);
		ParamNode* dst = nodes.getElement(insertAt + i);
		dst->pos = src->pos + oldLength;
		dst->value = src->value;
		dst->interpolated = src->interpolated;
	}
}

void AutoParam::nudgeNonInterpolatingNodesAtPos(int32_t, int32_t, int32_t, Action*, ModelStackWithAutoParam const*) {}

void AutoParam::stealNodes(ModelStackWithAutoParam const*, int32_t, int32_t, int32_t, Action*, StolenParamNodes*) {}

void AutoParam::insertStolenNodes(ModelStackWithAutoParam const*, int32_t, int32_t, int32_t, Action*,
                                  StolenParamNodes*) {}

void AutoParam::moveRegionHorizontally(ModelStackWithAutoParam const*, int32_t, int32_t, int32_t, int32_t, Action*) {}

void AutoParam::deleteNodesWithinRegion(ModelStackWithAutoParam const*, int32_t, int32_t) {}

int32_t AutoParam::homogenizeRegion(ModelStackWithAutoParam const*, int32_t, int32_t, int32_t, bool, bool, int32_t,
                                    bool, int32_t) {
	return 0;
}

int32_t AutoParam::getDistanceToNextNode(ModelStackWithAutoParam const*, int32_t, bool) {
	return 0;
}

void AutoParam::setCurrentValueWithNoReversionOrRecording(ModelStackWithAutoParam const*, int32_t value) {
	currentValue = value;
}

int32_t AutoParam::getValuePossiblyAtPos(int32_t, ModelStackWithAutoParam*) {
	return currentValue;
}

// ── Private methods ──────────────────────────────────────────────────────

bool AutoParam::deleteRedundantNodeInLinearRun(int32_t, int32_t, bool) {
	return false;
}

void AutoParam::setupInterpolation(ParamNode*, int32_t, int32_t, bool) {}

void AutoParam::homogenizeRegionTestSuccess(int32_t, int32_t, int32_t, bool, bool) {}

void AutoParam::deleteNodesBeyondPos(int32_t pos) {
	int32_t i = nodes.search(pos, GREATER_OR_EQUAL);
	int32_t numToDelete = nodes.getNumElements() - i;
	if (numToDelete) {
		nodes.deleteAtIndex(i, numToDelete);
	}
}
