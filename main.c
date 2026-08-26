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
// Run:
//   ./test_cjson

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "JSONC.h"

// Some inputs below trigger a genuine infinite loop in the current
// array_reader (see write-up). Running each parse in a forked child and
// killing it on timeout lets the suite keep going and report every other
// test, without corrupting the parent's heap the way an in-process
// signal+longjmp out of malloc would.

// ---------------------------------------------------------------------
// Tree dumper
// ---------------------------------------------------------------------

static const char* type_name(enum JSONC_t t) {
    switch (t) {
        case STRING:  return "STRING";
        case NUMBER:  return "NUMBER";
        case BOOLEAN: return "BOOLEAN";
        case NULL_e:  return "NULL";
        case ARRAY:   return "ARRAY";
        case OBJECT:  return "OBJECT";
        default:      return "UNKNOWN";
    }
}

static void indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

// Dumps a node and its `next` siblings at the same depth, recursing into
// `child` for objects/arrays. Guards against a couple of failure shapes
// we've seen (unset type, self-referential next) so a broken tree prints
// something informative instead of looping forever or segfaulting.
static void dump_node(JSONC* node, int depth, int max_siblings) {
    int count = 0;
    while (node && count < max_siblings) {
        indent(depth);
        printf("- type=%s", type_name(node->type));
        if (node->string) printf(" name=\"%s\"", node->string);

        switch (node->type) {
            case STRING:
                printf(" value=\"%s\"", node->valuestring ? node->valuestring : "(null)");
                break;
            case NUMBER:
                printf(" value=%g", node->valuedouble);
                break;
            case BOOLEAN:
                printf(" value=%s", node->valuedouble ? "true" : "false");
                break;
            case NULL_e:
                printf(" value=null");
                break;
            default:
                break;
        }
        printf("\n");

        if ((node->type == OBJECT || node->type == ARRAY) && node->child) {
            dump_node(node->child, depth + 1, 50);
        }

        if (node->next == node) {
            indent(depth);
            printf("  !! next points to self, stopping traversal\n");
            break;
        }
        node = node->next;
        count++;
    }
    if (count >= max_siblings) {
        indent(depth);
        printf("  !! stopped after %d siblings (possible infinite chain)\n", max_siblings);
    }
}

// ---------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------

static int tests_run = 0;
static int tests_parsed = 0;

static JSONC* parse_string(const char* json_text) {
    FILE* tmp = tmpfile();
    if (!tmp) { perror("tmpfile"); return NULL; }
    fwrite(json_text, 1, strlen(json_text), tmp);
    rewind(tmp);
    JSONC* result = json_in_c(tmp);
    fclose(tmp);
    return result;
}

#define TIMEOUT_SECS 2

// Runs `body` in a forked child so an infinite loop or crash in the
// parser can't take down the whole suite or corrupt the parent's heap.
// Waits up to TIMEOUT_SECS, polling; kills the child on timeout.
// Returns: 0 = child exited normally (its own output already printed),
//          1 = timed out and child was killed,
//          2 = child crashed (signal).
static int run_in_child(void (*body)(const char*), const char* arg) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        body(arg);
        _exit(0);
    }

    int status;
    for (int waited_ms = 0; waited_ms < TIMEOUT_SECS * 1000; waited_ms += 50) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) return WIFSIGNALED(status) ? 2 : 0;
        usleep(50 * 1000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    return 1;
}

static void run_test_body(const char* json_text) {
    JSONC* result = parse_string(json_text);
    if (result) {
        printf("result: parsed\n");
        dump_node(result, 1, 50);
    } else {
        printf("result: FAILED (NULL)\n");
    }
    fflush(stdout); // required: child exits via _exit(), which does NOT
                     // flush stdio buffers, so unflushed output here
                     // would otherwise silently vanish
}

static void run_test(const char* name, const char* json_text) {
    tests_run++;
    printf("=== [%s] ===\n", name);
    printf("input: %s\n", json_text);

    int rc = run_in_child(run_test_body, json_text);
    if (rc == 1) {
        printf("result: TIMED OUT (killed after %ds -- likely infinite loop)\n", TIMEOUT_SECS);
    } else if (rc == 2) {
        printf("result: CRASHED\n");
    } else {
        tests_parsed++; // child ran to completion; its own output was already printed
    }
    printf("\n");
}

// Same as run_test, but also runs a caller-supplied check against the
// result and reports pass/fail for a specific known-bug scenario.
typedef int (*check_fn)(JSONC* result);

// Set just before forking so the checked-test child can see which check
// to run (globals are copied into the child's address space by fork()).
static check_fn g_pending_check;

static void run_test_checked_body(const char* json_text) {
    JSONC* result = parse_string(json_text);
    if (result) {
        dump_node(result, 1, 50);
    } else {
        printf("result: FAILED (NULL)\n");
    }
    int ok = g_pending_check(result);
    printf("check: %s\n", ok ? "PASS" : "FAIL");
    fflush(stdout); // see comment in run_test_body
}

static void run_test_checked(const char* name, const char* json_text,
                              check_fn check, const char* expectation) {
    tests_run++;
    printf("=== [%s] ===\n", name);
    printf("input: %s\n", json_text);
    printf("expectation: %s\n", expectation);

    g_pending_check = check;
    int rc = run_in_child(run_test_checked_body, json_text);
    if (rc == 1) {
        printf("result: TIMED OUT (killed after %ds -- likely infinite loop)\n", TIMEOUT_SECS);
        printf("check: FAIL\n");
    } else if (rc == 2) {
        printf("result: CRASHED\n");
        printf("check: FAIL\n");
    } else {
        tests_parsed++;
    }
    printf("\n");
}

// ---------------------------------------------------------------------
// Checks for specific known bugs
// ---------------------------------------------------------------------

// Expects [1,2,3] to have three linked NUMBER nodes: 1 -> 2 -> 3.
// Currently fails because array_reader never advances `parent` to the
// newly-created node, so the 3rd element overwrites the link to the 2nd.
static int check_array_three_elements(JSONC* result) {
    if (!result) return 0;
    if (result->type != ARRAY || !result->child) return 0;
    JSONC* n1 = result->child;
    if (!n1 || n1->type != NUMBER || n1->valuedouble != 1) return 0;
    JSONC* n2 = n1->next;
    if (!n2 || n2->type != NUMBER || n2->valuedouble != 2) return 0;
    JSONC* n3 = n2->next;
    if (!n3 || n3->type != NUMBER || n3->valuedouble != 3) return 0;
    return 1;
}

// Expects {} to parse into a node with type == OBJECT (and no children).
// Currently object_reader rejects {} outright (returns NULL).
static int check_empty_object(JSONC* result) {
    if (!result) return 0;
    return result->type == OBJECT && result->child == NULL;
}

// Expects [] to parse into a node with type == ARRAY (and no children).
// Currently "succeeds" but leaves type as STRING (calloc-zeroed default)
// instead of ARRAY.
static int check_empty_array(JSONC* result) {
    if (!result) return 0;
    return result->type == ARRAY && result->child == NULL;
}

// Expects {"a":true,"b":false} to parse both booleans correctly.
// Regression test for the literal-fallthrough bug (now fixed, but worth
// keeping as a guard against it coming back).
static int check_two_booleans(JSONC* result) {
    if (!result) return 0;
    if (result->type != OBJECT || !result->child) return 0;

    JSONC* a = result->child;
    if (!a || a->type != BOOLEAN) return 0;
    if (strcmp(a->string, "a") != 0 || a->valuedouble != 1) return 0;

    JSONC* b = a->next;
    if (!b || b->type != BOOLEAN) return 0;
    if (strcmp(b->string, "b") != 0 || b->valuedouble != 0) return 0;

    return 1;
}

// ---------------------------------------------------------------------
// main
// ---------------------------------------------------------------------

int main(void) {

    printf("############################################\n");
    printf("# 1. Basic valid documents\n");
    printf("############################################\n\n");
    run_test("simple_object", "{\"a\":1}");
    run_test_checked("array_three_elements", "[1,2,3]",
                      check_array_three_elements,
                      "child chain should be 1 -> 2 -> 3");
    run_test("object_three_fields", "{\"a\":1,\"b\":2,\"c\":3}");
    run_test("top_level_string", "\"just a string\"");
    run_test("top_level_number", "42");
    run_test("nested_object", "{\"nested\":{\"x\":1}}");
    run_test("array_of_arrays", "[[1,2],[3,4]]");

    printf("############################################\n");
    printf("# 2. Empty containers\n");
    printf("############################################\n\n");
    run_test_checked("empty_object", "{}", check_empty_object,
                      "type should be OBJECT with no children");
    run_test_checked("empty_array", "[]", check_empty_array,
                      "type should be ARRAY with no children");
    run_test("object_with_empty_object", "{\"a\":{}}");

    printf("############################################\n");
    printf("# 3. Strings & escapes\n");
    printf("############################################\n\n");
    run_test("plain_string", "{\"a\":\"hello\"}");
    run_test("escaped_newline", "{\"a\":\"line1\\nline2\"}");
    run_test("escaped_quote", "{\"a\":\"quote\\\"inside\"}");
    run_test("escaped_backslash", "{\"a\":\"back\\\\slash\"}");
    run_test("unicode_escape", "{\"a\":\"emoji\\uD83D\\uDE00\"}");
    run_test("unterminated_string", "{\"a\":\"unterminated");
    {
        // Build a string value > 100 chars to probe the fixed-size buffer.
        char big[512];
        strcpy(big, "{\"a\":\"");
        size_t base = strlen(big);
        for (int i = 0; i < 150; i++) big[base + i] = 'x';
        strcpy(big + base + 150, "\"}");
        run_test("long_string_150_chars", big);
    }

    printf("############################################\n");
    printf("# 4. Numbers\n");
    printf("############################################\n\n");
    run_test("positive_int", "{\"a\":123}");
    run_test("negative_int", "{\"a\":-45}");
    run_test("zero", "{\"a\":0}");
    run_test("leading_zero_invalid", "{\"a\":01}");
    run_test("float", "{\"a\":3.14}");
    run_test("exponent", "{\"a\":1e10}");
    run_test("negative_exponent", "{\"a\":1.5e-3}");
    run_test("leading_plus_invalid", "{\"a\":+5}");
    run_test("no_leading_digit_invalid", "{\"a\":.5}");
    {
        // 20+ exponent digits to probe the 3-slot expo[] buffer.
        char big[512];
        strcpy(big, "{\"a\":1e");
        size_t base = strlen(big);
        for (int i = 0; i < 25; i++) big[base + i] = '1';
        strcpy(big + base + 25, "}");
        run_test("huge_exponent_25_digits", big);
    }

    printf("############################################\n");
    printf("# 5. Literals\n");
    printf("############################################\n\n");
    run_test("literal_true", "{\"a\":true}");
    run_test("literal_false", "{\"a\":false}");
    run_test("literal_null", "{\"a\":null}");
    run_test_checked("two_booleans_regression", "{\"a\":true,\"b\":false}",
                      check_two_booleans,
                      "both booleans parse correctly (fallthrough bug regression test)");
    run_test("malformed_literal", "{\"a\":tru}");

    printf("############################################\n");
    printf("# 6. Whitespace tolerance\n");
    printf("############################################\n\n");
    run_test("spaces_everywhere", "{ \"a\" : 1 }");
    run_test("newlines_and_indentation", "{\n  \"a\": 1\n}");
    run_test("leading_whitespace_before_brace", "   {\"a\":1}");

    printf("############################################\n");
    printf("# 7. Malformed structure (should fail cleanly)\n");
    printf("############################################\n\n");
    run_test("trailing_comma", "{\"a\":1,}");
    run_test("missing_comma", "{\"a\":1 \"b\":2}");
    run_test("leading_comma", "{,\"a\":1}");
    run_test("missing_colon_and_value", "{\"a\"}");
    run_test("empty_file", "");
    run_test("unterminated_object", "{");

    printf("############################################\n");
    printf("# Summary\n");
    printf("############################################\n");
    printf("%d/%d test inputs produced a non-NULL parse result\n", tests_parsed, tests_run);
    printf("(non-NULL doesn't mean correct -- check each dump against its expectation above)\n");

    return 0;
}
