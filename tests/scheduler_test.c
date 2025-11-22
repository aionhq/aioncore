/**
 * Host-side unit tests for scheduler
 *
 * Tests the O(1) scheduler logic without needing full kernel context.
 * Focuses on:
 * - Priority bitmap operations
 * - Task queue enqueue/dequeue
 * - Highest priority selection
 */

#include "host_test.h"
#include <string.h>

// Mock task structure for testing
typedef struct task {
    uint32_t task_id;
    uint8_t priority;
    struct task* next;
    struct task* prev;
} task_t;

// Mock task queue
typedef struct {
    task_t* head;
    task_t* tail;
    uint32_t count;
} task_queue_t;

// Priority bitmap (8 x 32-bit = 256 bits, one per priority)
static uint32_t priority_bitmap[8];

/**
 * Find highest priority with ready tasks
 *
 * Uses bitmap to find highest set bit in O(1) time.
 * Returns 0 if no tasks ready (idle priority).
 */
static uint8_t find_highest_priority(void) {
    // Search from highest to lowest (index 7 down to 0)
    for (int i = 7; i >= 0; i--) {
        if (priority_bitmap[i]) {
            // Found a non-zero word, find highest bit
            // __builtin_clz counts leading zeros from MSB
            int bit = 31 - __builtin_clz(priority_bitmap[i]);
            return (uint8_t)(i * 32 + bit);
        }
    }
    return 0;  // No tasks ready, return idle priority
}

/**
 * Set priority bit
 */
static void set_priority_bit(uint8_t priority) {
    uint32_t word_idx = priority / 32;
    uint32_t bit_idx = priority % 32;
    priority_bitmap[word_idx] |= (1u << bit_idx);
}

/**
 * Clear priority bit
 */
static void clear_priority_bit(uint8_t priority) {
    uint32_t word_idx = priority / 32;
    uint32_t bit_idx = priority % 32;
    priority_bitmap[word_idx] &= ~(1u << bit_idx);
}

/**
 * Check if priority bit is set
 */
static bool is_priority_set(uint8_t priority) {
    uint32_t word_idx = priority / 32;
    uint32_t bit_idx = priority % 32;
    return (priority_bitmap[word_idx] & (1u << bit_idx)) != 0;
}

/**
 * Enqueue task in priority queue
 */
static void enqueue_task(task_queue_t* queue, task_t* task) {
    if (!queue->head) {
        // Empty queue
        queue->head = task;
        queue->tail = task;
        task->next = NULL;
        task->prev = NULL;
    } else {
        // Append to tail
        task->prev = queue->tail;
        task->next = NULL;
        queue->tail->next = task;
        queue->tail = task;
    }
    queue->count++;
}

/**
 * Dequeue task from priority queue
 */
static void dequeue_task(task_queue_t* queue, task_t* task) {
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        // Removing head
        queue->head = task->next;
    }

    if (task->next) {
        task->next->prev = task->prev;
    } else {
        // Removing tail
        queue->tail = task->prev;
    }

    task->next = NULL;
    task->prev = NULL;
    queue->count--;
}

// Test 1: Priority bitmap operations
TEST(priority_bitmap_set_clear) {
    memset(priority_bitmap, 0, sizeof(priority_bitmap));

    // Set various priorities
    set_priority_bit(0);
    set_priority_bit(128);
    set_priority_bit(255);

    TEST_ASSERT(is_priority_set(0), "Priority 0 should be set");
    TEST_ASSERT(is_priority_set(128), "Priority 128 should be set");
    TEST_ASSERT(is_priority_set(255), "Priority 255 should be set");
    TEST_ASSERT(!is_priority_set(1), "Priority 1 should not be set");
    TEST_ASSERT(!is_priority_set(127), "Priority 127 should not be set");

    // Clear priorities
    clear_priority_bit(128);
    TEST_ASSERT(!is_priority_set(128), "Priority 128 should be cleared");
    TEST_ASSERT(is_priority_set(0), "Priority 0 should still be set");
    TEST_ASSERT(is_priority_set(255), "Priority 255 should still be set");

    return 1;
}

// Test 2: Find highest priority (empty)
TEST(find_highest_priority_empty) {
    memset(priority_bitmap, 0, sizeof(priority_bitmap));

    uint8_t priority = find_highest_priority();
    TEST_ASSERT_EQ(priority, 0, "Empty bitmap should return priority 0 (idle)");

    return 1;
}

// Test 3: Find highest priority (single task)
TEST(find_highest_priority_single) {
    memset(priority_bitmap, 0, sizeof(priority_bitmap));

    set_priority_bit(100);
    uint8_t priority = find_highest_priority();
    TEST_ASSERT_EQ(priority, 100, "Should find priority 100");

    return 1;
}

// Test 4: Find highest priority (multiple tasks)
TEST(find_highest_priority_multiple) {
    memset(priority_bitmap, 0, sizeof(priority_bitmap));

    set_priority_bit(10);
    set_priority_bit(50);
    set_priority_bit(200);
    set_priority_bit(100);

    uint8_t priority = find_highest_priority();
    TEST_ASSERT_EQ(priority, 200, "Should find highest priority 200");

    return 1;
}

// Test 5: Find highest priority (edge cases)
TEST(find_highest_priority_edges) {
    memset(priority_bitmap, 0, sizeof(priority_bitmap));

    // Test priority 0 (lowest)
    set_priority_bit(0);
    TEST_ASSERT_EQ(find_highest_priority(), 0, "Should find priority 0");

    // Add priority 255 (highest)
    set_priority_bit(255);
    TEST_ASSERT_EQ(find_highest_priority(), 255, "Should find priority 255");

    // Clear 255, should go back to 0
    clear_priority_bit(255);
    TEST_ASSERT_EQ(find_highest_priority(), 0, "Should find priority 0 after clearing 255");

    return 1;
}

// Test 6: Task queue enqueue
TEST(task_queue_enqueue) {
    task_queue_t queue = {0};
    task_t task1 = { .task_id = 1, .priority = 10 };
    task_t task2 = { .task_id = 2, .priority = 10 };
    task_t task3 = { .task_id = 3, .priority = 10 };

    // Enqueue first task
    enqueue_task(&queue, &task1);
    TEST_ASSERT_EQ(queue.count, 1, "Queue should have 1 task");
    TEST_ASSERT(queue.head == &task1, "Head should be task1");
    TEST_ASSERT(queue.tail == &task1, "Tail should be task1");

    // Enqueue second task
    enqueue_task(&queue, &task2);
    TEST_ASSERT_EQ(queue.count, 2, "Queue should have 2 tasks");
    TEST_ASSERT(queue.head == &task1, "Head should still be task1");
    TEST_ASSERT(queue.tail == &task2, "Tail should be task2");
    TEST_ASSERT(task1.next == &task2, "task1.next should be task2");

    // Enqueue third task
    enqueue_task(&queue, &task3);
    TEST_ASSERT_EQ(queue.count, 3, "Queue should have 3 tasks");
    TEST_ASSERT(queue.tail == &task3, "Tail should be task3");

    return 1;
}

// Test 7: Task queue dequeue (head)
TEST(task_queue_dequeue_head) {
    task_queue_t queue = {0};
    task_t task1 = { .task_id = 1 };
    task_t task2 = { .task_id = 2 };
    task_t task3 = { .task_id = 3 };

    enqueue_task(&queue, &task1);
    enqueue_task(&queue, &task2);
    enqueue_task(&queue, &task3);

    // Dequeue head
    dequeue_task(&queue, &task1);
    TEST_ASSERT_EQ(queue.count, 2, "Queue should have 2 tasks");
    TEST_ASSERT(queue.head == &task2, "Head should be task2");
    TEST_ASSERT(queue.tail == &task3, "Tail should still be task3");

    return 1;
}

// Test 8: Task queue dequeue (tail)
TEST(task_queue_dequeue_tail) {
    task_queue_t queue = {0};
    task_t task1 = { .task_id = 1 };
    task_t task2 = { .task_id = 2 };
    task_t task3 = { .task_id = 3 };

    enqueue_task(&queue, &task1);
    enqueue_task(&queue, &task2);
    enqueue_task(&queue, &task3);

    // Dequeue tail
    dequeue_task(&queue, &task3);
    TEST_ASSERT_EQ(queue.count, 2, "Queue should have 2 tasks");
    TEST_ASSERT(queue.head == &task1, "Head should still be task1");
    TEST_ASSERT(queue.tail == &task2, "Tail should be task2");

    return 1;
}

// Test 9: Task queue dequeue (middle)
TEST(task_queue_dequeue_middle) {
    task_queue_t queue = {0};
    task_t task1 = { .task_id = 1 };
    task_t task2 = { .task_id = 2 };
    task_t task3 = { .task_id = 3 };

    enqueue_task(&queue, &task1);
    enqueue_task(&queue, &task2);
    enqueue_task(&queue, &task3);

    // Dequeue middle
    dequeue_task(&queue, &task2);
    TEST_ASSERT_EQ(queue.count, 2, "Queue should have 2 tasks");
    TEST_ASSERT(queue.head == &task1, "Head should still be task1");
    TEST_ASSERT(queue.tail == &task3, "Tail should still be task3");
    TEST_ASSERT(task1.next == &task3, "task1.next should be task3");
    TEST_ASSERT(task3.prev == &task1, "task3.prev should be task1");

    return 1;
}

// Test 10: All priorities in range 0-255
TEST(priority_range_coverage) {
    memset(priority_bitmap, 0, sizeof(priority_bitmap));

    // Set all even priorities
    for (int i = 0; i < 256; i += 2) {
        set_priority_bit(i);
    }

    // Verify all even priorities are set
    for (int i = 0; i < 256; i += 2) {
        TEST_ASSERT(is_priority_set(i), "Even priority should be set");
    }

    // Verify all odd priorities are NOT set
    for (int i = 1; i < 256; i += 2) {
        TEST_ASSERT(!is_priority_set(i), "Odd priority should not be set");
    }

    // Find highest should return 254
    TEST_ASSERT_EQ(find_highest_priority(), 254, "Highest should be 254");

    return 1;
}
