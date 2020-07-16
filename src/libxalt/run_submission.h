#ifndef RUN_SUBMISSION_H
#define RUN_SUBMISSION_H

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "xalt_obfuscate.h"

void run_submission(double t0, pid_t pid, pid_t ppid, double start_time, double end_time, double probability,
		    char* exec_path, int num_tasks, int num_gpus, const char* xalt_kind, const char* uuid_str,
		    const char* watermark, const char* usr_cmdline, FILE* my_stderr);

void pkgRecordTransmit(const char* uuid_str, const char* syshost, const char* transmission, FILE* my_stderr);

#endif //RUN_SUBMISSION_H
