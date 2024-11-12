#ifndef __ERRORS_H
#define __ERRORS_H

typedef enum {
    SUCCESS = 0,
    ERROR_READ = 1,
    ERROR_WRITE = 2,
    ERROR_PARAMETR_IS_NULL = 3,
    ERROR_NUMBER_PROCESS_IS_INCORECT = 4,
    ERROR_MASSAGE_MAGIC = 5,
    ERROR_READ_HEADER = 6,
    ERROR_WRITE_HEADER = 7,
    ERROR = 8
} error_code;

#endif
