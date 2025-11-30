// Host-side test for timer interrupt logic
#include "host_test.h"
#include <stdint.h>
#include <string.h>

// Mock structures
typedef struct {
    uint64_t ticks;
} per_cpu_data_t;

typedef struct {
    uint32_t task_id;
    uint8_t priority;
    uint64_t cpu_time_ticks;
} task_t;

typedef struct {
    uint32_t head;
    uint32_t count;
} task_queue_t;

typedef struct {
    uint64_t ticks;
    task_t* current_task;
    task_queue_t ready[256];
    int need_resched;
} scheduler_t;

// Global mocks
static per_cpu_data_t mock_cpu;
static scheduler_t mock_scheduler;
static task_t mock_bootstrap_task;
static task_t mock_test_task;

// Mock this_cpu()
static per_cpu_data_t* this_cpu(void) {
    return &mock_cpu;
}

// Simplified scheduler_tick() - same logic as kernel
static int scheduler_tick(void) {
    mock_scheduler.ticks++;

    task_t* current = mock_scheduler.current_task;
    if (!current) {
        return 0;  // Should NOT crash here!
    }

    current->cpu_time_ticks++;

    uint8_t priority = current->priority;
    if (mock_scheduler.ready[priority].count > 0) {
        mock_scheduler.need_resched = 1;
        return 1;
    }

    return 0;
}

// Simplified timer_interrupt_handler
static void timer_interrupt_handler(void) {
    per_cpu_data_t* cpu = this_cpu();
    cpu->ticks++;
    scheduler_tick();
}

TEST(timer_interrupt_with_null_current_task) {
    // Setup: NULL current task (like at boot)
    memset(&mock_scheduler, 0, sizeof(mock_scheduler));
    mock_scheduler.current_task = NULL;
    mock_cpu.ticks = 0;

    // Execute: Fire interrupt
    timer_interrupt_handler();

    // Verify: Should NOT crash, should increment CPU ticks
    TEST_ASSERT_EQ(mock_cpu.ticks, 1, "CPU ticks");
    TEST_ASSERT_EQ(mock_scheduler.ticks, 1, "Scheduler ticks");
}

TEST(timer_interrupt_with_bootstrap_task) {
    // Setup: Bootstrap task (ID 0xFFFFFFFF, priority 0)
    memset(&mock_scheduler, 0, sizeof(mock_scheduler));
    memset(&mock_bootstrap_task, 0, sizeof(mock_bootstrap_task));
    mock_bootstrap_task.task_id = 0xFFFFFFFF;
    mock_bootstrap_task.priority = 0;
    mock_scheduler.current_task = &mock_bootstrap_task;
    mock_cpu.ticks = 0;

    // Execute: Fire interrupt
    timer_interrupt_handler();

    // Verify: Should work, no crash
    TEST_ASSERT_EQ(mock_cpu.ticks, 1, "CPU ticks");
    TEST_ASSERT_EQ(mock_scheduler.ticks, 1, "Scheduler ticks");
    TEST_ASSERT_EQ(mock_bootstrap_task.cpu_time_ticks, 1, "Task CPU time");
}

TEST(timer_interrupt_sets_need_resched_when_tasks_ready) {
    // Setup: Test task at priority 128, another task ready at same priority
    memset(&mock_scheduler, 0, sizeof(mock_scheduler));
    memset(&mock_test_task, 0, sizeof(mock_test_task));
    mock_test_task.task_id = 1;
    mock_test_task.priority = 128;
    mock_scheduler.current_task = &mock_test_task;
    mock_scheduler.ready[128].count = 1;  // Another task waiting
    mock_scheduler.need_resched = 0;

    // Execute: Fire interrupt
    timer_interrupt_handler();

    // Verify: need_resched should be set
    TEST_ASSERT_EQ(mock_scheduler.need_resched, 1, "need_resched set");
}

TEST(timer_interrupt_does_not_set_need_resched_when_no_tasks) {
    // Setup: Test task at priority 128, NO other tasks ready
    memset(&mock_scheduler, 0, sizeof(mock_scheduler));
    memset(&mock_test_task, 0, sizeof(mock_test_task));
    mock_test_task.task_id = 1;
    mock_test_task.priority = 128;
    mock_scheduler.current_task = &mock_test_task;
    mock_scheduler.ready[128].count = 0;  // No other tasks
    mock_scheduler.need_resched = 0;

    // Execute: Fire interrupt
    timer_interrupt_handler();

    // Verify: need_resched should NOT be set
    TEST_ASSERT_EQ(mock_scheduler.need_resched, 0, "need_resched not set");
}

TEST(multiple_timer_interrupts_accumulate_ticks) {
    // Setup
    memset(&mock_scheduler, 0, sizeof(mock_scheduler));
    memset(&mock_test_task, 0, sizeof(mock_test_task));
    mock_test_task.priority = 128;
    mock_scheduler.current_task = &mock_test_task;

    // Execute: 100 timer interrupts
    for (int i = 0; i < 100; i++) {
        timer_interrupt_handler();
    }

    // Verify
    TEST_ASSERT_EQ(mock_cpu.ticks, 100, "CPU ticks after 100 interrupts");
    TEST_ASSERT_EQ(mock_scheduler.ticks, 100, "Scheduler ticks after 100");
    TEST_ASSERT_EQ(mock_test_task.cpu_time_ticks, 100, "Task CPU time after 100");
    return 1;  // Success
}
