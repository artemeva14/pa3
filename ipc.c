#include <unistd.h>
#include "ipc.h"
#include "errors.h"
#include "str.h"
#include "logging.h"

// Реализация функций fullread и fullwrite
static size_t fullread(size_t fd, void *buffer, size_t length) {
    int result = 0;
    char *buf = (char *) buffer;

    while (result < length) {
        ssize_t cur = read(fd, buf + result, length - result);
        if (cur < 0) {
            return -1;
        } else if (cur == 0) {
            break;
        }
        result += cur;
    }
    return result;
}

static size_t fullwrite(size_t fd, const void *buffer, size_t length) {
    ssize_t result = 0;
    const char *buf = (const char *) buffer;

    while (result < length) {
        ssize_t cur = write(fd, buf + result, length - result);
        if (cur < 0) {
            return -1;
        }
        result += cur;
    }
    return result;
}

// Отправка сообщения конкретному процессу
int send(void *self, local_id dst, const Message *msg) {
    matrix *str = (matrix *) self;

    if (str == NULL || msg == NULL) return -1;
    if (str->local_id == dst) return -1;
    if (str->local_id > MAX_PROCESS_ID || dst > MAX_PROCESS_ID) return -1;

    size_t write_fd = str->write_fds[str->local_id][dst];

    if (msg->s_header.s_magic != MESSAGE_MAGIC) return -1;

    if (fullwrite(write_fd, &msg->s_header, sizeof(MessageHeader)) != sizeof(MessageHeader)) {
        return -1;
    }

    if (msg->s_header.s_payload_len > 0) {
        if (fullwrite(write_fd, msg->s_payload, msg->s_header.s_payload_len) != msg->s_header.s_payload_len) {
            return -1;
        }
    }

    return msg->s_header.s_type;
}

// Отправка сообщения всем процессам
int send_multicast(void *self, const Message *msg) {
    matrix *str = (matrix *) self;
    int result = SUCCESS;

    if (str == NULL || msg == NULL) return -1;

    for (local_id i = 0; i < MAX_PROCESS_ID; i++) {
        if (i != str->local_id) {
            int res = send(str, i, msg);
            if (res != SUCCESS) {
                result = res;
            }
        }
    }

    return result;
}

// Получение сообщения от конкретного процесса
int receive(void *self, local_id from, Message *msg) {
    matrix *str = (matrix *) self;

    while (1) {
        int msg_result =
                read(
                    str->read_fds[from][str->local_id],
                    &msg->s_header,
                    sizeof(MessageHeader)
                );
        if (msg_result == 0
            || msg_result == -1) {
            continue;
        }
        if (msg->s_header.s_payload_len > 0) {
            while (1) {
                msg_result =
                        read(
                            str->read_fds[from][str->local_id],
                            &msg->s_payload,
                            msg->s_header.s_payload_len
                        );
                if (!(msg_result == 0
                      || msg_result == -1)) {
                    break;
                }
            }
        }
        return msg->s_header.s_type;
    }
}

int receive_any(void *self, Message *msg) {
    matrix *str = (matrix *) self;

    if (str == NULL || msg == NULL) return -1;
    while (1) {
        for (local_id i = 0; i < MAX_PROCESS_ID; i++) {
            if (i != str->local_id) {
                size_t read_fd = str->read_fds[i][str->local_id];
                ssize_t header_read = fullread(read_fd, &msg->s_header, sizeof(MessageHeader));
                if (header_read == sizeof(MessageHeader)) {
                    if (msg->s_header.s_magic == MESSAGE_MAGIC) {
                        if (msg->s_header.s_payload_len > 0) {
                            if (fullread(read_fd, msg->s_payload, msg->s_header.s_payload_len) == msg->s_header.
                                s_payload_len) {
                                return msg->s_header.s_type;
                            }
                        } else {
                            return msg->s_header.s_type;
                        }
                    }
                } else {
                    continue;
                }
            }
        }
    }
}
