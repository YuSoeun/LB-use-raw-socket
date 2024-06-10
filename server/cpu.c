#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

void get_cpu_usage(CpuUsage *cpu) {
    FILE *fp;
    char buf[1024];

    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fgets(buf, sizeof(buf), fp);
    sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
           &cpu->user, &cpu->nice, &cpu->system, &cpu->idle,
           &cpu->iowait, &cpu->irq, &cpu->softirq, &cpu->steal);

    fclose(fp);
}

double calculate_cpu_usage(CpuUsage *prev, CpuUsage *curr) {
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long curr_idle = curr->idle + curr->iowait;

    unsigned long long prev_non_idle = prev->user + prev->nice + prev->system + prev->irq + prev->softirq + prev->steal;
    unsigned long long curr_non_idle = curr->user + curr->nice + curr->system + curr->irq + curr->softirq + curr->steal;

    unsigned long long prev_total = prev_idle + prev_non_idle;
    unsigned long long curr_total = curr_idle + curr_non_idle;

    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;

    return (double)(total_diff - idle_diff) / total_diff * 100.0;
}