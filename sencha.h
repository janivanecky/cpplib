#pragma once

struct SenchaTable {
    char variable_names[100][100];
    float variable_values[100];
    int variable_count = 0;
};

namespace sencha {
    void add_or_set_variable(SenchaTable *var_table, char *name, float value);
    bool get_variable(SenchaTable *var_table, char *name, float *value);
    bool parse_line(char *line, SenchaTable *var_table);
};

#ifdef CPPLIB_SENCHA_IMPL
#include "sencha.cpp"
#endif