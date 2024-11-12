#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dlfcn.h>
#include "banking.h"
#include "errors.h"
#include "ipc.h"
#include "str.h"
#include "pa2345.h"
#include "logging.h"

matrix *str;

timestamp_t get_lamport_time() {
    return str->local_time;
}

void increase_latest_time(matrix* str, timestamp_t max_time) {
    if (str -> local_time < max_time) {
        str -> local_time = max_time;
    }
    str -> local_time++;
}

void transfer(void *parent_data, local_id src, local_id dst,
              balance_t amount) {
    Transfer *transfer = (Transfer *) parent_data;
    TransferOrder transfer_order = {
        src,
        dst,
        amount
    };
    transfer->transfer_order = transfer_order;
    Message transfer_message = {
        .s_header = {
            .s_local_time = get_lamport_time(),
            .s_magic = MESSAGE_MAGIC,
            .s_payload_len = sizeof(Transfer),
            .s_type = TRANSFER,
        },
    };
    memcpy(
        &transfer_message.s_payload,
        transfer,
        sizeof(Transfer)
    );
    local_id dst_id;
    if (transfer->local_id != src) {
        dst_id = src;
    } else {
        dst_id = dst;
    }
    write(str->write_fds[transfer->local_id][dst_id], &transfer_message,
          sizeof(MessageHeader) + transfer_message.s_header.s_payload_len);
    if (str->local_id == PARENT_ID) {
        Message ack_message;
        receive(str, dst, &ack_message);
        increase_latest_time(str, ack_message.s_header.s_local_time);
    }
}

void update_balance(int change, timestamp_t balance_update_time) {
    int history_len = str->balance_history.s_history_len;
    timestamp_t current_time = get_lamport_time();
    if (history_len == current_time) {
        if (change > 0) {
            if (current_time > balance_update_time) {
                for (timestamp_t time = balance_update_time; time < current_time; time++) {
                    str-> balance_history.s_history[time].s_balance_pending_in = change;
                }
            }
        }
        balance_t new_balance = str->balance_history.s_history[history_len - 1].s_balance + change;
        str->balance_history.s_history[history_len] = (BalanceState){
            .s_time = current_time,
            .s_balance = new_balance,
            .s_balance_pending_in = 0,
        };
        str->balance_history.s_history_len++;
    } else if (history_len == current_time + 1) {
        if (change > 0) {
            if (current_time > balance_update_time) {
                for (timestamp_t time = balance_update_time; time < current_time; time++) {
                    str-> balance_history.s_history[time].s_balance_pending_in = change;
                }
            }
        }
        str->balance_history.s_history[history_len - 1].s_balance += change;
    } else if (history_len < current_time) {
        balance_t last_balance = str->balance_history.s_history[history_len - 1].s_balance;
        for (int index = history_len; index < current_time; index++) {
            str->balance_history.s_history[index] = (BalanceState){
                .s_time = index,
                .s_balance = last_balance,
                .s_balance_pending_in = 0,
            };
        }
        if (change > 0) {
            if (current_time > balance_update_time) {
                for (timestamp_t time = balance_update_time; time < current_time; time++) {
                    str-> balance_history.s_history[time].s_balance_pending_in = change;
                }
            }
        }
        str->balance_history.s_history[current_time] = (BalanceState){
            .s_time = current_time,
            .s_balance = last_balance + change,
            .s_balance_pending_in = 0,
        };
        str->balance_history.s_history_len = current_time + 1;
    }
}

void make_transfer(Message *msg, void* self) {
    matrix *str = (matrix *) self;
    timestamp_t balance_update_time = msg->s_header.s_local_time;
    Transfer *transfer_to_make = (Transfer *) msg->s_payload;
    TransferOrder transfer_order = transfer_to_make->transfer_order;
    if (str->local_id == transfer_order.s_dst) {
        update_balance(transfer_order.s_amount, balance_update_time);
        str->local_time++;
        Message ack_message = {
            .s_header = {
                .s_magic = MESSAGE_MAGIC,
                .s_type = ACK,
                .s_local_time = get_lamport_time(),
                .s_payload_len = 0,
            },
        };
        ack_message.s_header.s_payload_len = strlen(ack_message.s_payload);
        send(str, PARENT_ID, &ack_message);
    } else {
        update_balance(-transfer_order.s_amount, balance_update_time);
        transfer_to_make->local_id = str->local_id;
        transfer(
            transfer_to_make,
            transfer_order.s_src,
            transfer_order.s_dst,
            transfer_order.s_amount
        );
    }
}

void congregate_history(const void *source) {
    AllHistory *all_history = (AllHistory *) source;
    Message balance_message;
    for (size_t i = 1; i <= all_history->s_history_len; i++) {
        int msg_type = -1;
        do {
            msg_type = receive(str, i, &balance_message);
        } while (msg_type != BALANCE_HISTORY);
        increase_latest_time(str, balance_message.s_header.s_local_time);
        BalanceHistory *balance_history = (BalanceHistory *) balance_message.s_payload;
        all_history->s_history[balance_history->s_id - 1] = *balance_history;
    }
}

balance_t initial_balances[MAX_PROCESS_ID + 1]; // для хранения начальных балансов клиентов

int main(int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[1], "-p") != 0) {
        return ERROR;
    }

    size_t q_childs = strtol(argv[2], NULL, 10);

    for (size_t i = 1; i <= q_childs; i++) {
        initial_balances[i] = strtol(argv[i + 2], NULL, 10);
    }

    size_t q_proc = q_childs + 1;

    start_logging();	
    str = malloc(sizeof(matrix));

    if (str == NULL) {
        return ERROR;
    }

    for (size_t from = 0; from < q_proc; from++) {
        for (size_t dst = 0; dst < q_proc; dst++) {
            if (from != dst) {
                int fd[2];
                pipe(fd);
                fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL, 0) | O_NONBLOCK);
                fcntl(fd[1], F_SETFL, fcntl(fd[1], F_GETFL, 0) | O_NONBLOCK);
                str->read_fds[from][dst] = fd[0];
                str->write_fds[from][dst] = fd[1];
            }
        }
    }

    pid_t pr_pids[MAX_PROCESS_ID + 1];
    pr_pids[PARENT_ID] = getpid();
    for (size_t id = 1; id <= q_childs; id++) {
        pid_t child_id = fork();
        if (child_id == -1) {
            free(str);
            return ERROR;
        } else if (child_id == 0) {
            //ребенок
            str->balance_history = (BalanceHistory){
                .s_id = id,
                .s_history[0] = (BalanceState){
                    .s_balance_pending_in = 0,
                    .s_time = 0,
                    .s_balance = initial_balances[id],
                },
                .s_history_len = 1,
            };
            str->local_id = id;
            break;
        } else {
            //родитель
            pr_pids[id] = child_id;
            str->local_id = PARENT_ID;
        }
    }

    local_id cur_pid = str->local_id;
    for (size_t from = 0; from < q_proc; from++) {
        for (size_t dst = 0; dst < q_proc; dst++) {
            if (from != cur_pid && dst != cur_pid && from != dst) {
                close(str->read_fds[from][dst]);
                close(str->write_fds[from][dst]);
            }
            if (cur_pid == from && cur_pid != dst) {
                close(str->read_fds[from][dst]);
            }
            if (cur_pid == dst && cur_pid != from) {
                close(str->write_fds[from][dst]);
            }
        }
    }
    str->local_time = 0;
    if (str->local_id == PARENT_ID) {
        for (size_t from = 1; from <= q_childs; from++) {
            Message msg;
            receive(str, from, &msg);
            increase_latest_time(str, msg.s_header.s_local_time);
        }
        Transfer msg_transfer = {.local_id = str->local_id,};
        bank_robbery(&msg_transfer, q_childs);

        str -> local_time++;
        Message stop_message = {
            .s_header = {
                .s_magic = MESSAGE_MAGIC,
                .s_type = STOP,
                .s_payload_len = 0,
                .s_local_time = get_lamport_time(),
            },
        };
        send_multicast(str, &stop_message);

        Message msg;
        for (int from = 1; from <= q_childs; from++) {
            receive(str, from, &msg);
            increase_latest_time(str, msg.s_header.s_local_time);
        }

        AllHistory all_history = {.s_history_len = q_childs,};
        congregate_history(&all_history);
        print_history(&all_history);
    } else {
        str -> local_time++;
        Message started_message = {
            .s_header = {
                .s_magic = MESSAGE_MAGIC,
                .s_type = STARTED,
                .s_local_time = get_lamport_time(),
                .s_payload_len = sprintf(
                    started_message.s_payload,
                    log_started_fmt,
                    get_lamport_time(),
                    str->local_id,
                    getpid(),
                    getppid(),
                    str->balance_history.s_history[str->balance_history.s_history_len - 1].s_balance)
            },
        };
        send_multicast(str, &started_message);

        for (size_t from = 1; from <= q_childs; from++) {
            Message msg;
            if (from == cur_pid) continue;
            receive(str, from, &msg);
            increase_latest_time(str, msg.s_header.s_local_time);
        }

        int msg_type = -1;
        size_t ch = q_childs - 1;
        Message msg;
        while (msg_type != STOP) {
            str -> local_time++;
            msg_type = receive_any(str, &msg);
            increase_latest_time(str, msg.s_header.s_local_time);
            if (msg_type == TRANSFER) {
                make_transfer(&msg, str);
            } else if (msg_type == DONE) {
                ch--;
            } else if (msg_type == STOP) {
                Message done_message = {
                    .s_header = {
                        .s_magic = MESSAGE_MAGIC,
                        .s_type = DONE,
                        .s_local_time = get_lamport_time(),
                    },
                };
                int payload_len = sprintf(
                    done_message.s_payload,
                    log_done_fmt,
                    get_lamport_time(),
                    str -> local_id,
                    str -> balance_history.s_history[str->balance_history.s_history_len - 1].s_balance
                );
                done_message.s_header.s_payload_len = payload_len;
                send_multicast(str, &done_message);
            }
        }

        msg_type = -1;
        while (ch > 0) {
            str -> local_time++;
            msg_type = receive_any(str, &msg);
            increase_latest_time(str, msg.s_header.s_local_time);
            if (msg_type == TRANSFER) {
                make_transfer(&msg, str);
            } else if (msg_type == DONE) {
                ch--;
            }
        }
        str -> local_time++;
        Message history_message = {
            .s_header = {
                .s_type = BALANCE_HISTORY,
                .s_local_time = get_lamport_time(),
                .s_magic = MESSAGE_MAGIC,
                .s_payload_len = sizeof(BalanceHistory),
            },
        };
        memcpy(
            &history_message.s_payload,
            &str->balance_history,
            sizeof(BalanceHistory)
        );
        send(str, PARENT_ID, &history_message);
    }

    if (cur_pid == PARENT_ID) {
        for (size_t from = 1; from <= q_childs; from++) {
            waitpid(pr_pids[from], NULL, 0);
        }
    }
    
    finish_logging();
    free(str);
    return 0;
}
