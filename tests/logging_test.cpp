#include "logging.h"

int main(int argc, char **argv) {

    log_print("log_print call! %d", 1);
    log_error("log_error call! %d", 1);
    return 0;
}