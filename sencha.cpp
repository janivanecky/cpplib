#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include "maths.h"
#include "sencha.h"

// NOTE: For some messed up reason, name `TokenType` would collide with something
// inside Windows' headers so we have to go with this nicer, more expressive name.
enum SenchaTokenType {
    NUMBER,
    IDENTIFIER,
    OPERATOR,
    LEFT_PAREN,
    RIGHT_PAREN,
    COMMA,
    END_OF_FILE,
    END_OF_LINE,
};

struct Token {
    SenchaTokenType type;
    char *data;
    int data_size;
};

struct Lexer {
    char *current_ptr;
    char *base_ptr;
    int total_size;
};

char error_buffer[200];
void sencha_log_error(char *message) {
    int message_length = int(strlen(message));
    memcpy(error_buffer, message, message_length);
    error_buffer[message_length] = 0;
}

bool is_alpha(char c) {
    bool result = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
    return result;
}

bool is_num(char c) {
    bool result = (c >= '0') && (c <= '9');
    return result;
}

bool is_operator(char c) {
    bool result = (c == '+' || c == '-' || c == '=' || c == '*' || c == '/' || c == '^');
    return result;
}

bool get_variable(SenchaTable *var_table, Token token, float *value) {
    for(int i = 0; i < var_table->variable_count; ++i) {
        int variable_length = int(strlen(var_table->variable_names[i]));
        if(variable_length == token.data_size && strncmp(token.data, var_table->variable_names[i], token.data_size) == 0) {
            *value = var_table->variable_values[i];
            return true;
        }
    }
    return false;
}

void add_or_set_variable(SenchaTable *var_table, Token token, float value) {
    for(int i = 0; i < var_table->variable_count; ++i) {
        int variable_length = int(strlen(var_table->variable_names[i]));
        if(variable_length == token.data_size && strncmp(token.data, var_table->variable_names[i], token.data_size) == 0) {
            var_table->variable_values[i] = value;
            return;
        }
    }
    memcpy(var_table->variable_names[var_table->variable_count], token.data, token.data_size);
    var_table->variable_names[var_table->variable_count][token.data_size] = 0;
    var_table->variable_values[var_table->variable_count++] = value;
}

Token get_next_token(Lexer *lexer) {
    while(true) {
        if(lexer->current_ptr - lexer->base_ptr >= lexer->total_size) {
            Token result = Token{SenchaTokenType::END_OF_FILE, 0, 0};
            return result;
        }
        
        char current_char = *(lexer->current_ptr++);
        if(current_char == '\n') {
            Token result = Token{SenchaTokenType::END_OF_LINE, 0, 0};
            return result;
        }
        else if(is_num(current_char)) {
            Token result = Token{SenchaTokenType::NUMBER, lexer->current_ptr - 1, 1};
            current_char = *lexer->current_ptr;
            bool decimal_used = false;
            while(is_num(current_char) || (current_char == '.' && !decimal_used)) {
                if(current_char == '.') decimal_used = true;
                current_char = *(++lexer->current_ptr);
                result.data_size++;
            }
            if(current_char == 'f') {
                lexer->current_ptr++;
            }
            return result;
        } else if(is_alpha(current_char)) {
            Token result = Token{SenchaTokenType::IDENTIFIER, lexer->current_ptr - 1, 1};
            current_char = *lexer->current_ptr;
            while(is_alpha(current_char)) {
                current_char = *(++lexer->current_ptr);
                result.data_size++;
            }
            return result;
        } else if(is_operator(current_char)) {
            Token result = Token{SenchaTokenType::OPERATOR, lexer->current_ptr - 1, 1};
            return result;
        } else if(current_char == '(') {
            Token result = Token{SenchaTokenType::LEFT_PAREN, 0, 0};
            return result;
        } else if(current_char == ')') {
            Token result = Token{SenchaTokenType::RIGHT_PAREN, 0, 0};
            return result;
        } else if(current_char == ',') {
            Token result = Token{SenchaTokenType::COMMA, 0, 0};
            return result;
        }
    }
}

Token peek_next_token(Lexer *lexer) {
    char *original_ptr = lexer->current_ptr;
    Token result = get_next_token(lexer);
    lexer->current_ptr = original_ptr;
    return result;
}

bool parse_expression(Lexer *lexer, Token first_token, float *value, int precendence_level, SenchaTable *var_table) {
    float val = 0.0f;
    if(first_token.type == SenchaTokenType::END_OF_FILE || first_token.type == SenchaTokenType::END_OF_LINE) {
        sencha_log_error("Unexpected end of line, col TODO.");
        return false;
    }
    if(first_token.type == SenchaTokenType::OPERATOR) {
        sencha_log_error("Unexpected operator TODO, col TODO.");
        return false;
    }
    if(first_token.type == SenchaTokenType::LEFT_PAREN) {
        Token token = get_next_token(lexer);
        bool success = parse_expression(lexer, token, &val, 0, var_table);
        if(!success) {
            return false;
        }
        token = get_next_token(lexer);
        if(token.type != SenchaTokenType::RIGHT_PAREN) {
            sencha_log_error("Right parenthesis expected at col TODO.");
            return false;
        }
    } else {
        if (first_token.type == SenchaTokenType::NUMBER) {
            val = float(atof(first_token.data));
        }
        else if (first_token.type == SenchaTokenType::IDENTIFIER) {
            Token token = peek_next_token(lexer);
            if(token.type == SenchaTokenType::LEFT_PAREN) {
                get_next_token(lexer);
                Token token = get_next_token(lexer);
                bool success = parse_expression(lexer, token, &val, 0, var_table);
                if(!success) {
                    return false;
                }
                token = get_next_token(lexer);
                if(token.type != SenchaTokenType::RIGHT_PAREN) {
                    sencha_log_error("Right parenthesis expected at col TODO.");
                    return false;
                }
                if(strncmp(first_token.data, "sin", first_token.data_size) == 0) {
                    val = math::sin(val);
                } else if(strncmp(first_token.data, "cos", first_token.data_size) == 0) {
                    val = math::cos(val);
                } else {
                    sencha_log_error("Unknown function TODO at col TODO.");
                    return false;
                }
            } else {
                bool success = get_variable(var_table, first_token, &val);
                if(!success) {
                    sencha_log_error("Undefined variable TODO at col TODO.");
                    return false;
                }
            }
        }
    }
    Token token = peek_next_token(lexer);
    while(token.type != SenchaTokenType::END_OF_FILE && token.type != SenchaTokenType::END_OF_LINE) {
        if(token.type == SenchaTokenType::OPERATOR) {
            switch (token.data[0]) {
                case '+':
                if(precendence_level < 1) {
                    get_next_token(lexer);
                    Token next_token = get_next_token(lexer);
                    float temp_val = 0.0f;
                    bool success = parse_expression(lexer, next_token, &temp_val, 1, var_table);
                    if(!success) {
                        return false;
                    }
                    val += temp_val;
                } else {
                    *value = val;
                    return true;
                }
                break;
                case '-':
                if(precendence_level < 1) {
                    get_next_token(lexer);
                    Token next_token = get_next_token(lexer);
                    float temp_val = 0.0f;
                    bool success = parse_expression(lexer, next_token, &temp_val, 1, var_table);
                    if(!success) {
                        return false;
                    }
                    val -= temp_val;
                } else {
                    *value = val;
                    return true;
                }
                break;
                case '*':
                if(precendence_level < 2) {
                    get_next_token(lexer);
                    Token next_token = get_next_token(lexer);
                    float temp_val = 0.0f;
                    bool success = parse_expression(lexer, next_token, &temp_val, 2, var_table);
                    if(!success) {
                        return false;
                    }
                    val *= temp_val;
                } else {
                    *value = val;
                    return true;
                }
                break;
                case '/':
                if(precendence_level < 2) {
                    get_next_token(lexer);
                    Token next_token = get_next_token(lexer);
                    float temp_val = 0.0f;
                    bool success = parse_expression(lexer, next_token, &temp_val, 2, var_table);
                    if(!success) {
                        return false;
                    }
                    val /= temp_val;
                } else {
                    *value = val;
                    return true;
                }
                break;
            }
        } else if(token.type == SenchaTokenType::RIGHT_PAREN) {
            *value = val;
            return true;
        } else {
            sencha_log_error("Unexpected token TODO at col TODO.");
            return false;
        }
        token = peek_next_token(lexer);
    }
    *value = val;
    return true;
}

bool sencha::get_variable(SenchaTable *var_table, char *name, float *value) {
    int name_length = int(strlen(name));
    for(int i = 0; i < var_table->variable_count; ++i) {
        int variable_length = int(strlen(var_table->variable_names[i]));
        if(variable_length == name_length && strncmp(name, var_table->variable_names[i], name_length) == 0) {
            *value = var_table->variable_values[i];
            return true;
        }
    }
    return false;
}

void sencha::add_variable(SenchaTable *var_table, char *name, float value) {
    int name_length = int(strlen(name));
    memcpy(var_table->variable_names[var_table->variable_count], name, name_length);
    var_table->variable_names[var_table->variable_count][name_length] = 0;
    var_table->variable_values[var_table->variable_count++] = value;
}

bool sencha::parse_line(char *line, SenchaTable *var_table) {
    Lexer lexer = Lexer{
        line,
        line,
        int(strlen(line))
    };
    Token first_token = get_next_token(&lexer);
    
    if(first_token.type != SenchaTokenType::IDENTIFIER) {
        sencha_log_error("Unexpected token: TODO, col TODO. Line has to start with a variable name.");
        return false;
    }

    Token token = get_next_token(&lexer);
    if(token.type != SenchaTokenType::OPERATOR && token.data[0] != '=') {
        sencha_log_error("Missing '=' at col TODO. Line has to be in a form of assignment statement.");
        return false;
    }

    // Parse expresion here.
    token = get_next_token(&lexer);
    float v = 0.0f;
    bool success = parse_expression(&lexer, token, &v, 0, var_table);
    if(success) {
        add_or_set_variable(var_table, first_token, v);
    }
    return success;
}
