#ifndef __IFMO_DISTRIBUTED_CLASS_LOGGING_H
#define __IFMO_DISTRIBUTED_CLASS_LOGGING_H

#include <stdio.h>
#include "common.h"
#include "ipc.h"
#include <sys/types.h>
#include "banking.h"

void start_logging();
void finish_logging();
void log_started(timestamp_t time, local_id id, pid_t process_id, pid_t parent_process_id, balance_t balance);
void log_started_all(timestamp_t time, local_id id);
void log_done(timestamp_t time, local_id id, balance_t balance);
void log_done_all(timestamp_t time, local_id id);
void log_pipe_opened(int process_from, int process_to);
void log_transfer_in(timestamp_t time, local_id id, balance_t sum, local_id from);
void log_transfer_out(timestamp_t time, local_id id, balance_t sum, local_id to);
void log_situation(local_id id, int i);

#endif
