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

TEST(BDLinkedList, insertOtherNodeBefore) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&c);
	c.insertOtherNodeBefore(&b); // b goes before c

	auto* n = list.getFirst();
	CHECK_EQUAL(1, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	CHECK_EQUAL(2, static_cast<TestNode*>(n)->value);
	n = list.getNext(n);
	CHECK_EQUAL(3, static_cast<TestNode*>(n)->value);
	CHECK_EQUAL(3, list.getNum());
}

TEST(BDLinkedList, insertOtherNodeBeforeFirst) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2);
	list.addToEnd(&b);
	b.insertOtherNodeBefore(&a); // a goes before b (new first)

	POINTERS_EQUAL(&a, list.getFirst());
	CHECK_EQUAL(1, static_cast<TestNode*>(list.getFirst())->value);
	CHECK_EQUAL(2, list.getNum());
}

TEST(BDLinkedList, isLastTrue) {
	BidirectionalLinkedList list;
	TestNode a(1);
	list.addToEnd(&a);
	CHECK(a.isLast());
}

TEST(BDLinkedList, isLastFalse) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2);
	list.addToEnd(&a);
	list.addToEnd(&b);
	CHECK(!a.isLast());
	CHECK(b.isLast());
}

TEST(BDLinkedList, destructorRemovesFromList) {
	BidirectionalLinkedList list;
	TestNode a(1);
	{
		TestNode b(2);
		list.addToEnd(&a);
		list.addToEnd(&b);
		CHECK_EQUAL(2, list.getNum());
	} // b destroyed here, should auto-remove
	CHECK_EQUAL(1, list.getNum());
	POINTERS_EQUAL(&a, list.getFirst());
	CHECK(list.getNext(&a) == nullptr);
}

TEST(BDLinkedList, removeNotInListIsNoop) {
	TestNode a(1);
	a.remove(); // Should not crash — node not in any list
	CHECK(a.list == nullptr);
}

TEST(BDLinkedList, addToEndPreservesOrder) {
	BidirectionalLinkedList list;
	TestNode nodes[5] = {{10}, {20}, {30}, {40}, {50}};
	for (auto& n : nodes) {
		list.addToEnd(&n);
	}
	CHECK_EQUAL(5, list.getNum());

	auto* cur = list.getFirst();
	for (int i = 0; i < 5; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL((i + 1) * 10, static_cast<TestNode*>(cur)->value);
		cur = list.getNext(cur);
	}
	CHECK(cur == nullptr);
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
