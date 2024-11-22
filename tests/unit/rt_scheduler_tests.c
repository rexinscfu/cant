#include <assert.h>
#include <unistd.h>
#include "../../src/runtime/scheduler/rt_scheduler.h"

static void test_task(void* arg) {
    // Simulate some work
    usleep(1000);
}

static void test_basic_scheduling(void) {
    assert(scheduler_init());
    
    TaskConfig config = {
        .period_us = 10000,    // 10ms period
        .deadline_us = 5000,   // 5ms deadline
        .wcet_us = 2000,      // 2ms WCET
        .priority = TASK_PRIO_ENGINE,
        .entry_point = test_task,
        .arg = NULL,
        .name = "test_task"
    };
    
    assert(scheduler_create_task(&config));
    
    scheduler_start();
    sleep(1);  // Run for 1 second
    scheduler_stop();
    
    TaskStats stats = scheduler_get_task_stats("test_task");
    assert(stats.activation_count > 0);
    assert(stats.deadline_misses == 0);
}

int main(void) {
    test_basic_scheduling();
    printf("All real-time scheduler tests passed!\n");
    return 0;
} 