// Deep tests for BidirectionalLinkedList — stress, edge cases, re-insertion.

#include "CppUTest/TestHarness.h"
#include "util/container/list/bidirectional_linked_list.h"

class TestNode : public BidirectionalLinkedListNode {
public:
	int value;
	TestNode(int v = 0) : value(v) {}
};

TEST_GROUP(LinkedListDeep){};

TEST(LinkedListDeep, stressAddToEnd50Nodes) {
	BidirectionalLinkedList list;
	TestNode nodes[50];
	for (int i = 0; i < 50; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}
	CHECK_EQUAL(50, list.getNum());

	// Verify order
	BidirectionalLinkedListNode* cur = list.getFirst();
	for (int i = 0; i < 50; i++) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<TestNode*>(cur)->value);
		cur = list.getNext(cur);
	}
	POINTERS_EQUAL(nullptr, cur);
}

TEST(LinkedListDeep, stressAddToStart50Nodes) {
	BidirectionalLinkedList list;
	TestNode sentinel;
	sentinel.value = -1;
	list.addToEnd(&sentinel); // addToStart inserts after first

	TestNode nodes[50];
	for (int i = 0; i < 50; i++) {
		nodes[i].value = i;
		list.addToStart(&nodes[i]);
	}
	CHECK_EQUAL(51, list.getNum()); // 50 + sentinel

	// First is always sentinel, addToStart inserts after first
	BidirectionalLinkedListNode* cur = list.getFirst();
	CHECK_EQUAL(-1, static_cast<TestNode*>(cur)->value);
}

TEST(LinkedListDeep, removeMiddleNodes) {
	BidirectionalLinkedList list;
	TestNode nodes[10];
	for (int i = 0; i < 10; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	// Remove even-indexed nodes
	for (int i = 0; i < 10; i += 2) {
		nodes[i].remove();
	}
	CHECK_EQUAL(5, list.getNum());

	// Verify odd values remain in order
	BidirectionalLinkedListNode* cur = list.getFirst();
	for (int i = 1; i < 10; i += 2) {
		CHECK(cur != nullptr);
		CHECK_EQUAL(i, static_cast<TestNode*>(cur)->value);
		cur = list.getNext(cur);
	}
}

TEST(LinkedListDeep, removeAllFromFront) {
	BidirectionalLinkedList list;
	TestNode nodes[10];
	for (int i = 0; i < 10; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	for (int i = 0; i < 10; i++) {
		BidirectionalLinkedListNode* first = list.getFirst();
		CHECK(first != nullptr);
		first->remove();
	}
	CHECK_EQUAL(0, list.getNum());
	POINTERS_EQUAL(nullptr, list.getFirst());
}

TEST(LinkedListDeep, removeAllFromBack) {
	BidirectionalLinkedList list;
	TestNode nodes[10];
	for (int i = 0; i < 10; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}

	// Remove in reverse order
	for (int i = 9; i >= 0; i--) {
		nodes[i].remove();
	}
	CHECK_EQUAL(0, list.getNum());
	POINTERS_EQUAL(nullptr, list.getFirst());
}

TEST(LinkedListDeep, removeAndReinsert) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&b);
	list.addToEnd(&c);

	b.remove();
	CHECK_EQUAL(2, list.getNum());

	// Re-insert b at end
	list.addToEnd(&b);
	CHECK_EQUAL(3, list.getNum());

	BidirectionalLinkedListNode* cur = list.getFirst();
	CHECK_EQUAL(1, static_cast<TestNode*>(cur)->value);
	cur = list.getNext(cur);
	CHECK_EQUAL(3, static_cast<TestNode*>(cur)->value);
	cur = list.getNext(cur);
	CHECK_EQUAL(2, static_cast<TestNode*>(cur)->value);
	POINTERS_EQUAL(nullptr, list.getNext(cur));
}

TEST(LinkedListDeep, doubleRemoveIsNoop) {
	BidirectionalLinkedList list;
	TestNode a(1);
	list.addToEnd(&a);
	a.remove();
	a.remove(); // should be safe
	CHECK_EQUAL(0, list.getNum());
}

TEST(LinkedListDeep, insertBeforeFirstNode) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&b);

	// Insert c before a (the first node)
	a.insertOtherNodeBefore(&c);

	BidirectionalLinkedListNode* cur = list.getFirst();
	CHECK_EQUAL(3, static_cast<TestNode*>(cur)->value);
	cur = list.getNext(cur);
	CHECK_EQUAL(1, static_cast<TestNode*>(cur)->value);
	cur = list.getNext(cur);
	CHECK_EQUAL(2, static_cast<TestNode*>(cur)->value);
}

TEST(LinkedListDeep, insertBeforeLastNode) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&b);

	// Insert c before b (the last node)
	b.insertOtherNodeBefore(&c);

	BidirectionalLinkedListNode* cur = list.getFirst();
	CHECK_EQUAL(1, static_cast<TestNode*>(cur)->value);
	cur = list.getNext(cur);
	CHECK_EQUAL(3, static_cast<TestNode*>(cur)->value);
	cur = list.getNext(cur);
	CHECK_EQUAL(2, static_cast<TestNode*>(cur)->value);
}

TEST(LinkedListDeep, isLastWithMultipleNodes) {
	BidirectionalLinkedList list;
	TestNode a(1), b(2), c(3);
	list.addToEnd(&a);
	list.addToEnd(&b);
	list.addToEnd(&c);

	CHECK_FALSE(a.isLast());
	CHECK_FALSE(b.isLast());
	CHECK_TRUE(c.isLast());
}

TEST(LinkedListDeep, destructorAutoRemoves) {
	BidirectionalLinkedList list;
	TestNode a(1);
	{
		TestNode temp(99);
		list.addToEnd(&a);
		list.addToEnd(&temp);
		CHECK_EQUAL(2, list.getNum());
	} // temp destroyed here, should auto-remove

	CHECK_EQUAL(1, list.getNum());
	CHECK_EQUAL(1, static_cast<TestNode*>(list.getFirst())->value);
}

TEST(LinkedListDeep, moveNodeBetweenLists) {
	BidirectionalLinkedList list1, list2;
	TestNode a(1), b(2);
	list1.addToEnd(&a);
	list1.addToEnd(&b);

	// Move b from list1 to list2
	b.remove();
	list2.addToEnd(&b);

	CHECK_EQUAL(1, list1.getNum());
	CHECK_EQUAL(1, list2.getNum());
	CHECK_EQUAL(1, static_cast<TestNode*>(list1.getFirst())->value);
	CHECK_EQUAL(2, static_cast<TestNode*>(list2.getFirst())->value);
}

TEST(LinkedListDeep, testMethodDoesNotCrash) {
	BidirectionalLinkedList list;
	TestNode nodes[5];
	for (int i = 0; i < 5; i++) {
		nodes[i].value = i;
		list.addToEnd(&nodes[i]);
	}
	list.test(); // Should not crash or freeze
}
