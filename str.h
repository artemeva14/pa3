#ifndef __STR_H
#define __STR_H

#include "ipc.h"
#include "banking.h"

typedef struct {
    local_id local_id;     //локальный айдишник      
    int write_fds[MAX_PROCESS_ID+1][MAX_PROCESS_ID+1]; //каналы записи
    int read_fds[MAX_PROCESS_ID+1][MAX_PROCESS_ID+1];  //каналы чтения
    BalanceHistory balance_history;
    timestamp_t local_time;
} __attribute__((packed)) matrix;

typedef struct {
    local_id local_id;
    TransferOrder transfer_order;
} __attribute__((packed)) Transfer;

void increase_latest_time(matrix* str, timestamp_t max_time);

#endif
