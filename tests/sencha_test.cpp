
#define CPPLIB_MATHS_IMPL
#include "math.h"
#define CPPLIB_SENCHA_IMPL
#include "sencha.h"

#define ARRAYSIZE(x) int(sizeof(x) / sizeof(x[0]))

int main(int argc, char *argv[]) {
    struct TestCase {
        char *line;
        float result;
    };
    TestCase test_success[] = {
        TestCase{"x = 5 + (1)", 6.0f},
        TestCase{"x = sin(3.1415 / 2.0f)", math::sin(3.1415f / 2.0f)},
        TestCase{"x = sin(sin(0)) + 0.2f", 0.2f},
        TestCase{"x = sin(0 + cos(2))", math::sin(math::cos(2.0f))},
        TestCase{"x = sin(xx)", math::sin(1.0f)},
        TestCase{"x = y + z", 5.0f},
        TestCase{"x = 1 + 3 + 4 + 5", 13.0f},
        TestCase{"x = sin(t) * 0.25f + 0.75f", 0.75f},
    };
    printf("EXPECTED SUCCESS:\n");
    for(int i = 0; i < ARRAYSIZE(test_success); ++i) {
        SenchaTable var_table;
        sencha::add_variable(&var_table, "xx", 1.0f);
        sencha::add_variable(&var_table, "y", 2.0f);
        sencha::add_variable(&var_table, "z", 3.0f);
        sencha::add_variable(&var_table, "t", 0.0f);
        printf("%d: %s\n", i, test_success[i].line);
        bool result = sencha::parse_line(test_success[i].line, &var_table);
        if(!result) {
            printf("FAILED TO PARSE: %s\n", test_success[i].line);
            printf("%s\n", error_buffer);
            break;
        }
        float v;
        result = sencha::get_variable(&var_table, "x", &v);
        if(!result) {
            printf("FAILED TO GET VAR: %s\n", test_success[i].line);
            break;
        }
        if(v != test_success[i].result) {
            printf("WRONG RESULT %f, expected %f \n", v, test_success[i].result);
        }
    }
    char *test_fail[] = {
        "3 + 5",
        "x = ",
        "x = 3 3",
        "3 5",
        "x = +"
        "+ 3",
        "3 -",
        "(1 - 3)",
        "x = (+",
        "x = (3 + ",
        "x = (3 + 5",
        "x = sin(+5)",
        "x = sin + 5",
        "x = y + z"
        ""
    };
    printf("EXPECTED FAIL:\n");
    for(int i = 0; i < ARRAYSIZE(test_fail); ++i) {
        SenchaTable var_table;
        sencha::add_variable(&var_table, "z", 3.0f);
        printf("%d: %s\n", i + ARRAYSIZE(test_success), test_fail[i]);
        bool result = sencha::parse_line(test_fail[i], &var_table);
        if(result) {
            printf("SHOULD FAIL TO PARSE: %s\n", test_fail[i]);
        }
    }
    return 0;
}