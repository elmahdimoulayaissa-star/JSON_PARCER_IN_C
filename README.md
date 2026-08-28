JSONC

A minimal JSON parser in C. Parses a file into a linked-list/tree of JSONC nodes (objects/arrays use child + next/prev siblings).


API
```c
JSONC* json_in_c(FILE* file);   // parse a file into a JSONC tree (NULL on error)
void   print_JSONC(JSONC* head);// debug-print the tree
void   json_c_delete(JSONC* head); // free the tree
```
Usage
```c
#include "JSONC.h"

FILE* f = fopen("data.json", "r");
JSONC* root = json_in_c(f);
fclose(f);

if (root) {
    print_JSONC(root);
    json_c_delete(root);
}
```
Notes
Errors are printed to stderr/stdout; json_in_c returns NULL on parse failure.
No build system included — just compile JSONC.c with your project and link -lm.
