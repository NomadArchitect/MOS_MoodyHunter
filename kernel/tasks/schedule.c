// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.h"

#include "lib/structures/hashmap.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"

bool threads_foreach(const void *key, void *value)
{
    thread_id_t *tid = (thread_id_t *) key;
    thread_t *thread = (thread_t *) value;
    MOS_ASSERT_X(thread->id.thread_id == tid->thread_id, "something is wrong with the thread table");
    current_thread = thread;
    mos_platform->switch_to_thread(&current_cpu->scheduler_stack, thread);
    return true;
}

noreturn void cpu_do_schedule(void)
{
    while (1)
    {
        hashmap_foreach(thread_table, threads_foreach);
        pr_info("no more threads to schedule, starting over");
    }
}

void do_schedule(void)
{
    mos_platform->switch_to_scheduler(&current_thread->stack.head, current_cpu->scheduler_stack);
}
