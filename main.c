// test_cjson.c
//
// Minimal test harness for JSONC.c / JSONC.h.
// Since the parser has no JSONC_Print/getter API yet, this file includes
// its own small recursive dumper (built directly on the public struct
// fields) so each test's actual parsed tree is visible, not just
// "parsed / failed".
//
// Build:
//   gcc -Wall -Wextra -o test_cjson test_cjson.c JSONC.c -lm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "JSONC.h"
#include <time.h>

int main(int argc,char* argv[]){
    struct timespec start, end;
    FILE* file = fopen(argv[1],"r");
    clock_gettime(CLOCK_MONOTONIC, &start);

    JSONC* head = json_in_c(file);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed: %f seconds\n", elapsed);
    

    print_JSONC(head);

    json_c_delete(head);
    return 0;
}




