// Phase 10B: NamedThingVector tests
// Tests the sorted-by-name container. Note: insertElement(void*) auto-version
// and getName() use (uint32_t) pointer cast that truncates on 64-bit.
// We test by manually inserting elements via insertAtIndex + placement new.

#include "CppUTest/TestHarness.h"
#include "util/container/vector/named_thing_vector.h"
#include <cstring>
#include <new>

// A simple test struct that has a String at a known offset
struct TestNamedThing {
	int32_t data;
	String name;
};

// Helper: manually insert an element into the vector (bypassing getName() truncation)
static Error manualInsert(NamedThingVector& vec, int32_t index, void* thing, String* name) {
	Error err = vec.insertAtIndex(index);
	if (err != Error::NONE)
		return err;

	// Placement-new the element at the correct position
	void* addr = vec.getElementAddress(index);
	new (addr) NamedThingVectorElement(thing, name);
	return Error::NONE;
}

TEST_GROUP(NamedThingVector){};

TEST(NamedThingVector, constructorSetsStringOffset) {
	NamedThingVector vec(offsetof(TestNamedThing, name));
	CHECK_EQUAL(static_cast<int32_t>(offsetof(TestNamedThing, name)), vec.stringOffset);
	CHECK_EQUAL(0, vec.getNumElements());
}

TEST(NamedThingVector, searchEmptyVectorReturnsZero) {
	NamedThingVector vec(0);
	bool found = true;
	int32_t idx = vec.search("anything", GREATER_OR_EQUAL, &found);
	CHECK_EQUAL(0, idx);
	CHECK(!found);
}

TEST(NamedThingVector, searchEmptyVectorLessReturnsNegOne) {
	NamedThingVector vec(0);
	int32_t idx = vec.search("anything", LESS);
	CHECK_EQUAL(-1, idx);
}

TEST(NamedThingVector, insertAndSearchSingleElement) {
	NamedThingVector vec(offsetof(TestNamedThing, name));

	TestNamedThing thing;
	thing.data = 42;
	thing.name.set("alpha");

	Error err = manualInsert(vec, 0, &thing, &thing.name);
	CHECK_EQUAL(static_cast<int>(Error::NONE), static_cast<int>(err));
	CHECK_EQUAL(1, vec.getNumElements());

	bool found = false;
	int32_t idx = vec.search("alpha", GREATER_OR_EQUAL, &found);
	CHECK(found);
	CHECK_EQUAL(0, idx);
}

TEST(NamedThingVector, searchFindsCorrectIndex) {
	NamedThingVector vec(offsetof(TestNamedThing, name));

	TestNamedThing a, b, c;
	a.name.set("alpha");
	b.name.set("beta");
	c.name.set("gamma");

	// Insert in sorted order
	manualInsert(vec, 0, &a, &a.name);
	manualInsert(vec, 1, &b, &b.name);
	manualInsert(vec, 2, &c, &c.name);

	bool found = false;

	vec.search("alpha", GREATER_OR_EQUAL, &found);
	CHECK(found);

	vec.search("beta", GREATER_OR_EQUAL, &found);
	CHECK(found);

	vec.search("gamma", GREATER_OR_EQUAL, &found);
	CHECK(found);
}

TEST(NamedThingVector, searchNotFoundReturnInsertionPoint) {
	NamedThingVector vec(offsetof(TestNamedThing, name));

	TestNamedThing a, c;
	a.name.set("alpha");
	c.name.set("gamma");

	manualInsert(vec, 0, &a, &a.name);
	manualInsert(vec, 1, &c, &c.name);

	bool found = false;
	int32_t idx = vec.search("beta", GREATER_OR_EQUAL, &found);
	CHECK(!found);
	CHECK_EQUAL(1, idx); // "beta" would go between "alpha" and "gamma"
}

TEST(NamedThingVector, searchCaseInsensitive) {
	NamedThingVector vec(offsetof(TestNamedThing, name));

	TestNamedThing a;
	a.name.set("Alpha");

	manualInsert(vec, 0, &a, &a.name);

	bool found = false;
	vec.search("alpha", GREATER_OR_EQUAL, &found);
	CHECK(found);

	vec.search("ALPHA", GREATER_OR_EQUAL, &found);
	CHECK(found);
}

TEST(NamedThingVector, getElementReturnsNamedThing) {
	NamedThingVector vec(offsetof(TestNamedThing, name));

	TestNamedThing thing;
	thing.data = 99;
	thing.name.set("test");

	manualInsert(vec, 0, &thing, &thing.name);

	void* result = vec.getElement(0);
	POINTERS_EQUAL(&thing, result);
}

TEST(NamedThingVector, removeElementReducesCount) {
	NamedThingVector vec(offsetof(TestNamedThing, name));

	TestNamedThing a, b;
	a.name.set("alpha");
	b.name.set("beta");

	manualInsert(vec, 0, &a, &a.name);
	manualInsert(vec, 1, &b, &b.name);
	CHECK_EQUAL(2, vec.getNumElements());

	vec.removeElement(0);
	CHECK_EQUAL(1, vec.getNumElements());

	// Remaining element should be "beta"
	void* remaining = vec.getElement(0);
	POINTERS_EQUAL(&b, remaining);
}

TEST(NamedThingVector, removeLastElement) {
	NamedThingVector vec(offsetof(TestNamedThing, name));

	TestNamedThing a;
	a.name.set("only");

	manualInsert(vec, 0, &a, &a.name);
	CHECK_EQUAL(1, vec.getNumElements());

	vec.removeElement(0);
	CHECK_EQUAL(0, vec.getNumElements());
}
