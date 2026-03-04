// ParamNodeVector regression tests — exercises the firmware's real ParamNodeVector
// compiled on x86 with the mock memory allocator.

#include "CppUTest/TestHarness.h"
#include "modulation/params/param_node.h"
#include "modulation/params/param_node_vector.h"

TEST_GROUP(ParamNodeVectorTest) {};

TEST(ParamNodeVectorTest, emptyAfterConstruction) {
	ParamNodeVector vec;
	CHECK_EQUAL(0, vec.getNumElements());
	CHECK(vec.getFirst() == nullptr);
	CHECK(vec.getLast() == nullptr);
}

TEST(ParamNodeVectorTest, insertAndRetrieveNodes) {
	ParamNodeVector vec;

	// Insert nodes at positions 0, 100, 200
	for (int32_t pos = 0; pos < 3; pos++) {
		int32_t idx = vec.insertAtKey(pos * 100);
		CHECK(idx >= 0);
		ParamNode* node = (ParamNode*)vec.getElementAddress(idx);
		node->value = pos * 1000;
		node->interpolated = (pos % 2 == 0);
	}

	CHECK_EQUAL(3, vec.getNumElements());

	// Verify sorted order and values
	for (int32_t i = 0; i < 3; i++) {
		ParamNode* node = vec.getElement(i);
		CHECK(node != nullptr);
		CHECK_EQUAL(i * 100, node->pos);
		CHECK_EQUAL(i * 1000, node->value);
	}
}

TEST(ParamNodeVectorTest, insertMaintainsOrder) {
	ParamNodeVector vec;

	int32_t positions[] = {500, 100, 300, 700, 200};
	for (int i = 0; i < 5; i++) {
		int32_t idx = vec.insertAtKey(positions[i]);
		CHECK(idx >= 0);
		ParamNode* node = (ParamNode*)vec.getElementAddress(idx);
		node->value = positions[i];
	}

	CHECK_EQUAL(5, vec.getNumElements());

	// Verify sorted by position
	for (int32_t i = 0; i < vec.getNumElements() - 1; i++) {
		CHECK(vec.getElement(i)->pos <= vec.getElement(i + 1)->pos);
	}
}

TEST(ParamNodeVectorTest, searchExact) {
	ParamNodeVector vec;

	for (int32_t pos = 0; pos < 10; pos++) {
		int32_t idx = vec.insertAtKey(pos * 50);
		ParamNode* node = (ParamNode*)vec.getElementAddress(idx);
		node->value = pos;
	}

	int32_t idx = vec.searchExact(250); // position 250, inserted as 5th element
	CHECK(idx >= 0);
	ParamNode* node = vec.getElement(idx);
	CHECK_EQUAL(250, node->pos);
	CHECK_EQUAL(5, node->value);
}

TEST(ParamNodeVectorTest, searchMissing) {
	ParamNodeVector vec;

	vec.insertAtKey(100);
	vec.insertAtKey(200);
	vec.insertAtKey(300);

	int32_t idx = vec.searchExact(150);
	CHECK(idx < 0);
}

TEST(ParamNodeVectorTest, deleteNode) {
	ParamNodeVector vec;

	for (int32_t pos = 0; pos < 5; pos++) {
		int32_t idx = vec.insertAtKey(pos * 10);
		ParamNode* node = (ParamNode*)vec.getElementAddress(idx);
		node->value = pos;
	}

	vec.deleteAtKey(20); // Remove position 20
	CHECK_EQUAL(4, vec.getNumElements());
	CHECK(vec.searchExact(20) < 0);
	CHECK(vec.searchExact(10) >= 0);
	CHECK(vec.searchExact(30) >= 0);
}

TEST(ParamNodeVectorTest, getFirstAndLast) {
	ParamNodeVector vec;

	vec.insertAtKey(500);
	vec.insertAtKey(100);
	vec.insertAtKey(900);

	ParamNode* first = vec.getFirst();
	ParamNode* last = vec.getLast();

	CHECK(first != nullptr);
	CHECK(last != nullptr);
	CHECK_EQUAL(100, first->pos);
	CHECK_EQUAL(900, last->pos);
}

TEST(ParamNodeVectorTest, getElementBoundsCheck) {
	ParamNodeVector vec;

	CHECK(vec.getElement(-1) == nullptr);
	CHECK(vec.getElement(0) == nullptr);
	CHECK(vec.getElement(1) == nullptr);

	vec.insertAtKey(42);
	CHECK(vec.getElement(0) != nullptr);
	CHECK(vec.getElement(1) == nullptr);
	CHECK(vec.getElement(-1) == nullptr);
}

TEST(ParamNodeVectorTest, manyNodes) {
	ParamNodeVector vec;

	for (int32_t i = 0; i < 50; i++) {
		int32_t idx = vec.insertAtKey(i * 7); // Non-sequential positions
		CHECK(idx >= 0);
		ParamNode* node = (ParamNode*)vec.getElementAddress(idx);
		node->value = i;
		node->interpolated = true;
	}

	CHECK_EQUAL(50, vec.getNumElements());

	// Verify all nodes retrievable and in order
	for (int32_t i = 0; i < 50; i++) {
		ParamNode* node = vec.getElement(i);
		CHECK(node != nullptr);
		CHECK_EQUAL(i * 7, node->pos);
		CHECK_EQUAL(i, node->value);
		CHECK(node->interpolated);
	}
}

TEST(ParamNodeVectorTest, interpolationFlag) {
	ParamNodeVector vec;

	int32_t idx = vec.insertAtKey(0);
	ParamNode* node = (ParamNode*)vec.getElementAddress(idx);
	node->value = 42;
	node->interpolated = false;

	idx = vec.insertAtKey(100);
	node = (ParamNode*)vec.getElementAddress(idx);
	node->value = 84;
	node->interpolated = true;

	// Retrieve and verify
	ParamNode* n0 = vec.getElement(0);
	ParamNode* n1 = vec.getElement(1);

	CHECK_EQUAL(42, n0->value);
	CHECK(!n0->interpolated);
	CHECK_EQUAL(84, n1->value);
	CHECK(n1->interpolated);
}
