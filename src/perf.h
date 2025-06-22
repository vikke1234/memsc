#pragma once
#include <sys/types.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                       int cpu, int group_fd, unsigned long flags);
