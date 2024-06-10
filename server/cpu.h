#ifndef __CPU_H__
#define __CPU_H__

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} CpuUsage;

void get_cpu_usage(CpuUsage *cpu);
double calculate_cpu_usage(CpuUsage *prev, CpuUsage *curr);

#endif