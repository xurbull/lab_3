// Name: Kanan Aliyev 251ADB099
// https://github.com/your-username/calc-lab-repo
// Compile with: gcc -O2 -g -Wall -Wextra -std=c17 -fsanitize=address,undefined -o calc calc.c -lm
// Resources used: external sites(written as comments in the code)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

// --- Section 8 Required Explanations ---
// 1. How the tokenizer tracks positions: 
//    The lexer reads a single large buffer in memory. Position is calculated directly via 
//    pointer arithmetic/indexing: `cursor_idx + 1`. This gives a strict 1-based absolute position including spaces and newlines.
// 2. Why the grammar gives right precedence and why ** is right associative:
//    Recursive descent processes lowest precedence (+/-) at the highest function level (parse_expr),
//    so they evaluate last. '**' is parsed deeper. It is right-associative because parse_power() 
//    recursively calls itself for the right operand instead of using a while loop.
// 3. Why each error is reported at the position you chose:
//    The lexer attaches the starting 1-based index to every Token. If an error occurs, the parser 
//    simply reports the position stored inside the offending Token or tracks the operand's start token.
// 4. How you decide between integer and float arithmetic:
//    Values are wrapped in a struct tracking type. If a '.' is parsed, it's a float. During math operations, 
//    if either operand is a float, the other is promoted to double before the operation.
// 5. How you handle large files without slowing down:
//    Instead of reading line-by-line or copying strings, the entire file is loaded into a single 
//    dynamically allocated char array via fread(). The parser moves a single integer index through this array, meaning zero copies are made.

typedef enum {
    TOK_INT, TOK_FLOAT, TOK_PLUS, TOK_MINUS, 
    TOK_STAR, TOK_SLASH, TOK_POW, TOK_LPAREN, 
    TOK_RPAREN, TOK_NEWLINE, TOK_EOF, TOK_ERR
} TokenType;

typedef struct {
    TokenType type;
    size_t pos;
    long long i_val;
    double f_val;
} Token;

typedef struct {
    bool is_float;
    long long i;
    double f;
} Value;

typedef struct { 
    Value val; 
    size_t pos; 
} ExprRes;

// initial state
char *src_buffer = NULL;
size_t src_len = 0;
size_t cursor_idx = 0;
bool at_line_start = true;
Token current_token;
Value *results = NULL;
size_t res_count = 0;

bool has_error = false;
size_t global_error_pos = 0;

void set_error(size_t pos) {
    if (!has_error) {
        has_error = true;
        global_error_pos = pos;
    }
}

size_t current_pos() { 
    return cursor_idx + 1; 
}

// Tokenizer function to get the next token from the source buffer
Token get_next_token() {
    Token t = {0};

    // to skip the whitespaces and tabs before the token
    while (cursor_idx < src_len && (src_buffer[cursor_idx] == ' ' || src_buffer[cursor_idx] == '\t')) { 
        cursor_idx++;
    }

    // track the position of the token
    t.pos = current_pos();

    if (cursor_idx >= src_len) {
        t.type = TOK_EOF;
        return t;
    }

    char c = src_buffer[cursor_idx];

    // handle newlines: flag to true to know about newline
    if (c == '\n') {
        at_line_start = true;
        cursor_idx++;
        t.type = TOK_NEWLINE;
        return t;
    }

    if (c == '#') {
        if (at_line_start) {
            // skip the line since it's a comment
            while (cursor_idx < src_len && src_buffer[cursor_idx] != '\n') {
                cursor_idx++;
            }
            return get_next_token();
        } else {
            // '#' is in the middle of a line, so we give an error
            set_error(t.pos);
            t.type = TOK_ERR;
            return t;
        }
    }

    // to know that we are in the middle of a line
    at_line_start = false;

    // handle numbers (integers and floats)
    if (isdigit(c)) {
        long long int_val = 0;

        // parse, but not by char but take the whole number until a non-digit is found
        while (cursor_idx < src_len && isdigit(src_buffer[cursor_idx])) {
            // to convert the character to integer(https://stackoverflow.com/questions/71106548/converting-multi-digit-char-to-int-in-c)
            int_val = int_val * 10 + (src_buffer[cursor_idx] - '0');
            cursor_idx++;
        }
        // check for a decimal part to determine if it is a float or not
        if (cursor_idx < src_len && src_buffer[cursor_idx] == '.') {
            // parsing the number including the decimal part, used external source(https://stackoverflow.com/questions/85223/how-to-manually-parse-a-floating-point-number-from-a-string)
            if (cursor_idx + 1 >= src_len || !isdigit(src_buffer[cursor_idx + 1])) {
                // Trailing dot error
                set_error(cursor_idx + 1);
                t.type = TOK_ERR;
                return t;
            }
            cursor_idx++;
            
            double f_val = (double)int_val;
            double divisor = 1.0;
            
            while (cursor_idx < src_len && isdigit(src_buffer[cursor_idx])) {
                f_val = f_val * 10.0 + (src_buffer[cursor_idx] - '0');
                divisor *= 10.0;
                cursor_idx++;
            }
            
            t.type = TOK_FLOAT;
            t.f_val = f_val / divisor;
        } else {
            t.type = TOK_INT;
            t.i_val = int_val;
        }
        return t;
    }

    // handle the case where a dot is found without a leading digit
    if (c == '.') {
        set_error(t.pos);
        t.type = TOK_ERR;
        return t;
    }

    // handle operators and parentheses
    if (c == '+') { 
        t.type = TOK_PLUS; cursor_idx++; return t; 
    }
    if (c == '-') { 
        t.type = TOK_MINUS; cursor_idx++; return t; 
    }
    if (c == '*') {
        // we check for power('**')
        if (cursor_idx + 1 < src_len && src_buffer[cursor_idx + 1] == '*') {
            t.type = TOK_POW; cursor_idx += 2; return t;
        }
        t.type = TOK_STAR; cursor_idx++; return t;
    }
    if (c == '/') { 
        t.type = TOK_SLASH; cursor_idx++; return t; 
    }
    if (c == '(') { 
        t.type = TOK_LPAREN; cursor_idx++; return t; 
    }
    if (c == ')') { 
        t.type = TOK_RPAREN; cursor_idx++; return t; 
    }

    set_error(t.pos);
    t.type = TOK_ERR;
    return t;
}

// if there is an error, we output it. And continue otherwise
void advance() {
    if (!has_error) {
        current_token = get_next_token();
    }   
}

long long int_pow(long long base, long long exp) {
    long long res = 1;
    while (exp > 0) {
        if (exp % 2 == 1) res *= base;
        base *= base;
        exp /= 2;
    }
    return res;
}

ExprRes parse_expr();

ExprRes parse_primary() {
    ExprRes res = {0};
    res.pos = current_token.pos;
    if (has_error) return res;

    if (current_token.type == TOK_INT) {
        res.val.is_float = false; res.val.i = current_token.i_val;
        advance();
    } else if (current_token.type == TOK_FLOAT) {
        res.val.is_float = true; res.val.f = current_token.f_val;
        advance();
    } else if (current_token.type == TOK_LPAREN) {
        size_t lparen_pos = current_token.pos; 
        advance();
        res = parse_expr();
        res.pos = lparen_pos;                  
        if (current_token.type != TOK_RPAREN) {
            set_error(current_token.pos);
        } else {
            advance();
        }
    } else {
        set_error(current_token.pos);
    }
    return res;
}

Value eval_op(Value left, Value right, TokenType op, size_t op_pos, size_t right_pos) {
    Value res = {0};
    if (has_error){
        return res;
    }

    // Determine if the result should be a float based on the operands
    res.is_float = left.is_float || right.is_float;

    double lf;
    if (left.is_float) {
        lf = left.f;
    } else {
        lf = (double)left.i;
    }

    double rf;
    if (right.is_float) {
        rf = right.f;
    } else {
        rf = (double)right.i;
    }

    if (op == TOK_PLUS) {
        if (res.is_float) {
            res.f = lf + rf;
        }
        else {
            res.i = left.i + right.i;
        }
    } else if (op == TOK_MINUS) {
        if (res.is_float) {
            res.f = lf - rf;
        } else {
            res.i = left.i - right.i;
        }
    } else if (op == TOK_STAR) {
        if (res.is_float) {
            res.f = lf * rf;
        } else {
            res.i = left.i * right.i;
        }
    } else if (op == TOK_SLASH) {
        // Check for division by zero
        if ((right.is_float && right.f == 0.0) || (!right.is_float && right.i == 0)) {
            set_error(right_pos); return res;
        }
        if (res.is_float) {
           res.f = lf / rf; 
        }
        else {
            res.i = left.i / right.i;
        }        
    } else if (op == TOK_POW) {
        if (!res.is_float) {
            // integer
            if (right.i < 0) { 
                set_error(right_pos);
                return res; 
            }
            res.i = int_pow(left.i, right.i);
        } else {
            if (lf < 0.0 && floor(rf) != rf) {
                set_error(op_pos); 
                return res;
            }

            res.f = pow(lf, rf);

            if (!isfinite(res.f)) {
                set_error(op_pos); 
                return res;
            }
        }
    }
    return res;
}

// Used external site for left/right associativity (https://invisibleunicorns.com/posts/2022/05/01/recursive_descent_precedence_and_associativity/)
// hadles the right associative power operator '**'. It is the lowest level and has to be done first
ExprRes parse_power() {
    ExprRes left = parse_primary();
    if (has_error){
        return left;
    }

    // since ** is right associative, we use an if statement and recursion to handle the right side first
    if (current_token.type == TOK_POW) {
        Token op = current_token;
        advance();
   
        ExprRes right = parse_power();
        if (has_error){
            return right;
        } 

        left.val = eval_op(left.val, right.val, op.type, op.pos, right.pos);
    }
    return left;
}

// handles the left associative * and / operators. It is the second level and has to be done after power
ExprRes parse_term() {
    ExprRes left = parse_power();
    if (has_error) {
        return left;
    } 

    // since * and / are left associative, we use a while loop to handle multiple operations immediatly
    while (current_token.type == TOK_STAR || current_token.type == TOK_SLASH) {
        Token op = current_token;
        advance();

        ExprRes right = parse_power();
        if (has_error) {
            return right;
        } 

        left.val = eval_op(left.val, right.val, op.type, op.pos, right.pos);
    }
    return left;
}

// handles the left associative + and - operators. It is the highest level and has to be done last
ExprRes parse_expr() {
    ExprRes left = parse_term();
    if (has_error) {
        return left;
    } 

    while (current_token.type == TOK_PLUS || current_token.type == TOK_MINUS) {
        Token op = current_token;
        advance();
        ExprRes right = parse_term();
        if (has_error) {
            return right;
        }
        left.val = eval_op(left.val, right.val, op.type, op.pos, right.pos);
    }
    return left;
}

void process_file(const char *in_path, const char *out_path) {
    FILE *f = fopen(in_path, "rb");
    if (!f) {
        return;
    } 
    
    // to get size of the file
    // putting the pointer at the end of the file, getting the position, and then putting it back to the start
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // read file to allocated memory
    src_buffer = malloc(sz + 1);
    if (fread(src_buffer, 1, sz, f) != (size_t)sz) {
        sz = strlen(src_buffer);
    }
    src_buffer[sz] = '\0';
    src_len = sz;
    fclose(f);

    cursor_idx = 0;
    at_line_start = true;
    has_error = false;
    global_error_pos = 0;
    res_count = 0;
    
    advance();

    results = malloc(sizeof(Value) * 1000000);

    // parse each line until EOF or error
    while (current_token.type != TOK_EOF && !has_error) {
        if (current_token.type == TOK_NEWLINE) {
            advance();
            continue;
        }

        // calulate this line and store it in the temporary array
        // check for errors after the expression
        ExprRes res = parse_expr();
        if (has_error) {
            break;
        }

        // check for trailing characters after the expression
        if (current_token.type != TOK_NEWLINE && current_token.type != TOK_EOF) {
            set_error(current_token.pos);
            break;
        }

        // store the result in the result array
        results[res_count++] = res.val;

        if (current_token.type == TOK_NEWLINE) {
            advance();
        }
    }

    // output the results to the output file
    FILE *out = fopen(out_path, "w");
    if (out) {
        if (has_error) {
            fprintf(out, "ERROR:%zu\n", global_error_pos);
        } else {
            for (size_t i = 0; i < res_count; i++) {
                if (results[i].is_float) {
                    fprintf(out, "%.15g\n", results[i].f);
                } else {
                    fprintf(out, "%lld\n", results[i].i);
                }
            }
        }
        fclose(out);
    }

    free(results);
    free(src_buffer);
}

// used external site(https://stackoverflow.com/questions/7180293/how-to-extract-filename-from-path)
// extracts the base name of a file from a given path, removing the directory and the .txt extension if present
void extract_basename(const char *path, char *base) {
    const char *last_slash = strrchr(path, '/');

    const char *start;
    if (last_slash != NULL) {
        start = last_slash + 1;
    } else {
        start = path;
    }

    const char *ext = strrchr(start, '.');
    if (ext && strcmp(ext, ".txt") == 0) {
        strncpy(base, start, ext - start);
        base[ext - start] = '\0';
    } else {
        strcpy(base, start);
    }
}

int main(int argc, char **argv) {
    #define NAME_ID "kanan_aliyev_251adb099"

    char *in_dir = NULL;
    char *out_dir = NULL;
    char *in_file = NULL;

    // parse command line arguments
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-dir") == 0) && i + 1 < argc) {
            in_dir = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_dir = argv[++i];
        } else {
            in_file = argv[i];
        }
    }

    if (!in_dir && !in_file) {
        fprintf(stderr, "Missing input file or directory\n");
        return 1;
    }

    if (in_dir) {
        DIR *d = opendir(in_dir);
        if (!d) {
            fprintf(stderr, "Cannot open directory %s\n", in_dir);
            return 1;
        }

        // create the output directory with permissions for read, write, execute
        if (out_dir) {
            mkdir(out_dir, 0777);
        }
        
        // used external site(https://stackoverflow.com/questions/308695/how-do-i-concatenate-const-literal-strings-in-c)
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
        // check if the file has a .txt extension and is at least 4 characters long
            if (strstr(ent->d_name, ".txt") && strlen(ent->d_name) >= 4 && strcmp(ent->d_name + strlen(ent->d_name) - 4, ".txt") == 0) {
                
                char in_path[1024];
                snprintf(in_path, sizeof(in_path), "%s/%s", in_dir, ent->d_name);
                
                char base[512];
                extract_basename(ent->d_name, base);
                
                char out_path[4096];
                if (out_dir) {
                    snprintf(out_path, sizeof(out_path), "%s/%s_%s.txt", out_dir, base, NAME_ID);
                } else {
                    char def_dir[2048];
                    snprintf(def_dir, sizeof(def_dir), "%s_%s", base, NAME_ID);
                    mkdir(def_dir, 0777);
                    snprintf(out_path, sizeof(out_path), "%s/%s_%s.txt", def_dir, base, NAME_ID);
                }
                process_file(in_path, out_path);
            }
        }
        closedir(d);
    } else if (in_file) {
        char base[512];
        extract_basename(in_file, base);
        
        if (out_dir) {
            mkdir(out_dir, 0777);
        }

        char out_path[4096];
        if (out_dir) {
            snprintf(out_path, sizeof(out_path), "%s/%s_%s.txt", out_dir, base, NAME_ID);
        } else {
            char def_dir[2048];
            snprintf(def_dir, sizeof(def_dir), "%s_%s", base, NAME_ID);
            mkdir(def_dir, 0777);
            snprintf(out_path, sizeof(out_path), "%s/%s_%s.txt", def_dir, base, NAME_ID);
        }
        process_file(in_file, out_path);
    }

    return 0;
}