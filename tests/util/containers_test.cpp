// Phase 9B: Container data structure tests
// BidirectionalLinkedList, QuickSorter, OpenAddressingHashTable

#include "CppUTest/TestHarness.h"
#include "util/algorithm/quick_sorter.h"
#include "util/container/hashtable/open_addressing_hash_table.h"
#include "util/container/list/bidirectional_linked_list.h"
#include <cstring>

// ── BidirectionalLinkedList ─────────────────────────────────────────────

// Test node subclass to store data
class TestNode : public BidirectionalLinkedListNode {
public:
	int value;
	TestNode(int v = 0) : value(v) {}
};

TEST_GROUP(BDLinkedList){};

TEST(BDLinkedList, constructEmpty) {
	BidirectionalLinkedList list;
	CHECK(list.getFirst() == nullptr);
	CHECK_EQUAL(0, list.getNum());
}

TEST(BDLinkedList, addToEndAndGetFirst) {
	BidirectionalLinkedList list;
	TestNode node(42);
	list.addToEnd(&node);
	POINTERS_EQUAL(&node, list.getFirst());
}

TEST(BDLinkedList, addToEndGetNum) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&b);
	list.addToEnd(&c);
	CHECK_EQUAL(3, list.getNum());
}

TEST(BDLinkedList, addToStartInsertsAfterFirst) {
	// addToStart inserts after the current first node (not before)
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&c);
	list.addToStart(&b); // Inserts b after a

	// Order: a → b → c
	auto* n = list.getFirst();
	CHECK_EQUAL(1, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	CHECK_EQUAL(2, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	CHECK_EQUAL(3, static_cast<TestNode*>(n)->value);
	CHECK_EQUAL(3, list.getNum());
}

TEST(BDLinkedList, getNextTraversal) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&b);
	list.addToEnd(&c);

	auto* n = list.getFirst();
	CHECK_EQUAL(1, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	CHECK_EQUAL(2, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	CHECK_EQUAL(3, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	POINTERS_EQUAL(nullptr, n);
}

TEST(BDLinkedList, nodeRemove) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&b);
	list.addToEnd(&c);

	b.remove();
	CHECK_EQUAL(2, list.getNum());

	auto* n = list.getFirst();
	CHECK_EQUAL(1, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	CHECK_EQUAL(3, static_cast<TestNode*>(n)->value);
}

TEST(BDLinkedList, removeFirstNode) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2);
	list.addToEnd(&a);
	list.addToEnd(&b);

	a.remove();
	POINTERS_EQUAL(&b, list.getFirst());
	CHECK_EQUAL(1, list.getNum());
}

TEST(BDLinkedList, removeOnlyNode) {
	BidirectionalLinkedList list;
	TestNode a(1);
	list.addToEnd(&a);
	a.remove();
	POINTERS_EQUAL(nullptr, list.getFirst());
	CHECK_EQUAL(0, list.getNum());
}

// ── QuickSorter ─────────────────────────────────────────────────────────
// NOTE: QuickSorter uses (uint32_t)memory pointer arithmetic which truncates
// 64-bit pointers, causing segfaults. Cannot test on x86-64.
// The source compiles with -fpermissive for link coverage only.

// ── OpenAddressingHashTable ─────────────────────────────────────────────
// NOTE: insert/lookup/remove crash on 64-bit because getBucketAddress() uses
// (uint32_t)memory pointer arithmetic which truncates 64-bit pointers.
// Only constructor tests are safe.

TEST_GROUP(HashTable){};

TEST(HashTable, construct32bit) {
	OpenAddressingHashTableWith32bitKey table;
	CHECK_EQUAL(0, table.numElements);
	POINTERS_EQUAL(nullptr, table.memory);
}

TEST(HashTable, construct16bit) {
	OpenAddressingHashTableWith16bitKey table;
	CHECK_EQUAL(0, table.numElements);
	POINTERS_EQUAL(nullptr, table.memory);
}

TEST(HashTable, construct8bit) {
	OpenAddressingHashTableWith8bitKey table;
	CHECK_EQUAL(0, table.numElements);
	POINTERS_EQUAL(nullptr, table.memory);
}
