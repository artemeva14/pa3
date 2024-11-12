#include "logging.h"
#include "pa2345.h"

static FILE *log_file_events = NULL;
static FILE *log_file_pipes = NULL;

void start_logging() {
    if ((log_file_events = fopen(events_log, "w")) == NULL || (log_file_pipes = fopen(pipes_log, "w")) == NULL) {
        perror("Error while opening files");
    }
}

void finish_logging() {
    fclose(log_file_events);
    fclose(log_file_pipes);
}

void log_started(timestamp_t time, local_id id, pid_t process_id, pid_t parent_process_id, balance_t balance) {
    printf(log_started_fmt, time, id, process_id, parent_process_id, balance);
    fprintf(log_file_events, log_started_fmt, time, id, process_id, parent_process_id, balance);
    fflush(stdout);
    fflush(log_file_events);
}

void log_started_all(timestamp_t time, local_id id) {
    printf(log_received_all_started_fmt, time, id);
    fprintf(log_file_events, log_received_all_started_fmt, time, id);
    fflush(stdout);
    fflush(log_file_events);
}

void log_done(timestamp_t time, local_id id, balance_t balance) {
    printf(log_done_fmt, time, id, balance);
    fprintf(log_file_events, log_done_fmt, time, id, balance);
    fflush(stdout);
    fflush(log_file_events);
}

void log_done_all(timestamp_t time, local_id id) {
    printf(log_received_all_done_fmt, time, id);
    fprintf(log_file_events, log_received_all_done_fmt, time, id);
    fflush(stdout);
    fflush(log_file_events);
}

void log_pipe_opened(int process_from, int process_to) {
    const char *format = "Opened pipe from process %1d to %1d\n";
    printf(format, process_from, process_to);
    fprintf(log_file_pipes, format, process_from, process_to);
    fflush(stdout);
    fflush(log_file_pipes);
}

void log_transfer_in(timestamp_t time, local_id id, balance_t sum, local_id from) {
    printf(log_transfer_in_fmt, time, id, sum, from);
    fprintf(log_file_events, log_transfer_in_fmt, time, id, sum, from);
    fflush(stdout);
    fflush(log_file_events);
}

void log_transfer_out(timestamp_t time, local_id id, balance_t sum, local_id to) {
    printf(log_transfer_out_fmt, time, id, sum, to);
    fprintf(log_file_events, log_transfer_out_fmt, time, id, sum, to);
    fflush(stdout);
    fflush(log_file_events);
}

void log_situation(local_id id, int i) {
    const char *format = "Situation %1d for process %1d\n";
    printf(format, i, id);
    fprintf(log_file_pipes, format, i, id);
    fflush(stdout);
    fflush(log_file_pipes);
}
