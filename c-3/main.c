#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024 * 100
#define MAX_TOKEN_LEN_PRINT 32

#define CHECK_ALLOC(x)                                                         \
    do {                                                                       \
        if (!(x)) {                                                            \
            printf("%s:%d: Memory allocation failed", __FILE__, __LINE__);     \
            abort();                                                           \
        }                                                                      \
    } while (0)

#define CHECK_BUFFER_OFERFLOW(offset, size)                                    \
    do {                                                                       \
        if ((offset) >= (size)) {                                              \
            printf("%s:%d: Buffer overflow", __FILE__, __LINE__);              \
            abort();                                                           \
        }                                                                      \
    } while (0)

typedef enum {
    STATUS_OK = 0,
    STATUS_PENDING = 1,
    STATUS_ERROR = 2,
} status_t;

typedef enum {
    TSTRAY = 0,   // Stray invalid token
    TCOMMA,       // ,
    TSEMICOLON,   // ;
    TLBRACE,      // {
    TRBRACE,      // }
    TASSIGN,      // =
    TOR,          // or
    TAND,         // and
    TEQ,          // ==
    TNE,          // !=
    TLE,          // <=
    TGE,          // >=
    TLT,          // <
    TGT,          // >
    TPLUS,        // +
    TMINUS,       // -
    TSTAR,        // *
    TSLASH,       // /
    TBANG,        // !
    TDOT,         // .
    TLPAREN,      // (
    TRPAREN,      // )
    TSTRING = 22, // "<alphanum>"
    TIDENTIFIER,  // <alphanum>
    TNUMBER,      // <num>
    TIF,          // if
    TFOR,         // for
    TFUN,         // fun
    TNIL,         // nil
    TVAR,         // var
    TELSE,        // else
    TTHIS,        // this
    TTRUE,        // true
    TCLASS,       // class
    TFALSE,       // false
    TPRINT,       // print
    TSUPER,       // super
    TWHILE,       // while
    TRETURN,      // return
} token_type_t;

typedef struct token {
    struct token *next;
    token_type_t type;
    size_t offset;
    size_t len;
} token_t;

enum lex_state {
    LIDLE,
    LCOMMENTBEGIN,
    LCOMMENT,
    LSTRING,
    LSTRINGESCAPE,
    LMULTICHAR,
    LIDENTIFIER,
    LNUMBER,
    LNUMBERDOTBEGIN,
    LNUMBERDOT,
};

typedef struct {
    enum lex_state state;
    bool has_err;
    token_t *first;
    token_t *pending_first;
    token_t *last;
    token_t *current;
    token_t *err_first;
    token_t *pending_err_first;
    token_t *err_last;
    char *source;
    size_t offset;
} lex_t;

typedef enum {
    EXMIN = 0,
    EXASSIGN,     // assign
    EXOR,         // binary or
    EXAND,        // binary and
    EXEQUAL,      // binary != ==
    EXCOMPAR,     // binary > >= < <=
    EXTERM,       // binary + -
    EXFACTOR,     // binary * /
    EXUNARY,      // unary
    EXCALL,       // call
    EXANON,       // anon
    EXTRUE,       // literal
    EXFALSE,      // literal
    EXNIL,        // literal
    EXTHIS,       // literal
    EXNUMBER,     // literal
    EXSTRING,     // literal
    EXIDENTIFIER, // literal
    EXSUPER,      // literal
    EXMAX,
} exprtype_t;

struct expression;

struct expr_assign {
    struct expression *to;
    struct expression *from;
};

typedef enum {
    BINARY_OR,
    BINARY_AND,
    BINARY_EQ,
    BINARY_NE,
    BINARY_LE,
    BINARY_GE,
    BINARY_LT,
    BINARY_GT,
    BINARY_SUM,
    BINARY_SUB,
    BINARY_MUL,
    BINARY_DIV,
} binexprtype_t;

struct expr_binary {
    binexprtype_t type;
    struct expression *left;
    struct expression *right;
};

typedef enum {
    UNARY_NEG,
    UNARY_INV,
} unary_type_t;

struct expr_unary {
    unary_type_t type;
    struct expression *value;
};

typedef enum {
    CALL_ID,   // <callee>.id
    CALL_CALL, // <callee>()
} call_type_t;

struct call_arg {
    struct call_arg *next;
    struct expression *expr;
};

struct expr_call {
    call_type_t type;
    token_t id;                // in case of CALL_ID and CALL_ID_CALL
    struct expression *callee; // <callee><call>
    struct call_arg *first;    // call args list
    struct call_arg *last;
};

struct expr_anon {
    struct expression *value;
};

typedef struct expression {
    exprtype_t type;
    union {
        struct expr_assign assign;
        struct expr_binary binary;
        struct expr_unary unary;
        struct expr_call call;
        struct expr_anon anon;
        token_t literal;
    };
} expression_t;

typedef enum {
    STEXPR,
} stmttype_t;

typedef struct {
    stmttype_t type;
    union {
        expression_t *expr;
    };
} statement_t;

enum pars_state {
    PSTMT,         // Expecting beginning of any statement type
    PEXPR,         // Expecting nested expression
    PANONEXPR,     // Expecting closing parenthesis of anon expression
    POPER,         // Expecting closing parenthesis, operator, or semicolon
    PCALLID,       // Expecting id after former dot
    PCALLARG,      // Expecting closing parenthesis or call arg
    PCALLARGDELIM, // Expecting closing parenthesis or call arg delimiter
    PSUPERDOT,     // Expecting dot `.` after `super` in `super.IDENTIFIER`
    PSUPERID,      // Expecting `IDENTIFIER` after `super` in `super.IDENTIFIER`
};

typedef struct pars_stack {
    struct pars_stack *next;
    union {
        expression_t *expr;
        statement_t *stmt;
    };
    enum pars_state state;
} pars_stack_frame_t;

typedef struct {
    pars_stack_frame_t *stack;
    bool has_err;
    token_t *err_token;
    char const *source;
} pars_t;

typedef struct {
    bool has_err;
    char const *source;
} interp_t;

void lex_init(lex_t *self, char *source) {
    *self = (lex_t){
        .source = source,
    };
}

#define KEYWORDS_COUNT 16

const struct {
    char const *keyword;
    size_t len;
    token_type_t type;
} keywords[KEYWORDS_COUNT] = {
    {"or", 2, TOR},
    {"if", 2, TIF},
    {"and", 3, TAND},
    {"for", 3, TFOR},
    {"fun", 3, TFUN},
    {"nil", 3, TNIL},
    {"var", 3, TVAR},
    {"else", 4, TELSE},
    {"this", 4, TTHIS},
    {"true", 4, TTRUE},
    {"class", 5, TCLASS},
    {"false", 5, TFALSE},
    {"print", 5, TPRINT},
    {"super", 5, TSUPER},
    {"while", 5, TWHILE},
    {"return", 6, TRETURN},
};

static inline size_t min(size_t a, size_t b) { return a < b ? a : b; }

static bool is_alpha(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool is_number(int c) { return (c >= '0' && c <= '9'); }

static bool is_single_char(int c) {
    return c == '{' || c == '}' || c == '(' || c == ')' || c == '=' ||
           c == '.' || c == '+' || c == '-' || c == '/' || c == '*' ||
           c == ';' || c == '<' || c == '>' || c == '!' || c == ',';
}

static bool is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_multichar_beginning(int c) {
    return c == '=' || c == '<' || c == '>' || c == '!';
}

static bool
is_operator_composition(char const *source, token_t const *token, int c) {
    char first = source[token->offset];
    return c == '=' &&
           (first == '=' || first == '!' || first == '<' || first == '>');
}

static bool is_keyword_composition(char const *source, token_t const *token) {
    assert(token->len > 0);
    if (token->len > 6)
        return false;
    size_t len = token->len;
    char const *w = source + token->offset;
    for (size_t i = 0; i < KEYWORDS_COUNT; i++) {
        char const *kw = keywords[i].keyword;
        size_t kwlen = keywords[i].len;
        if (kwlen == len && memcmp(w, kw, kwlen) == 0)
            return true;
    }
    return false;
}

static token_type_t token_type(int c) {
    switch (c) {
    case '{':
        return TLBRACE;
    case '}':
        return TRBRACE;
    case '(':
        return TLPAREN;
    case ')':
        return TRPAREN;
    case '=':
        return TASSIGN;
    case '+':
        return TPLUS;
    case '-':
        return TMINUS;
    case '*':
        return TSTAR;
    case '/':
        return TSLASH;
    case ';':
        return TSEMICOLON;
    case '.':
        return TDOT;
    case ',':
        return TCOMMA;
    case '<':
        return TLT;
    case '>':
        return TGT;
    case '!':
        return TBANG;
    case '"':
        return TSTRING;
    default:
        if (is_alpha(c))
            return TIDENTIFIER;
        else if (is_number(c))
            return TNUMBER;
        assert(false);
    };
    return 0;
};

static token_t *new_token(size_t offset, token_type_t type) {
    token_t *token = calloc(1, sizeof(token_t));
    CHECK_ALLOC(token);
    *token = (token_t){
        .type = type,
        .offset = offset,
        .len = 1,
    };
    return token;
}

static token_t *copy_token(token_t const *from) {
    token_t *token = calloc(1, sizeof(token_t));
    CHECK_ALLOC(token);
    *token = *from;
    return token;
}

static void finish_token(lex_t *self, token_t *token) {
    if (self->last)
        self->last->next = token;
    else
        self->first = token;
    self->last = token;
    if (!self->pending_first)
        self->pending_first = token;
}

static void add_token(lex_t *self, token_type_t type) {
    finish_token(self, new_token(self->offset, type));
}

static void begin_token(lex_t *self, int c) {
    self->current = new_token(self->offset, token_type(c));
}

static void continue_token(lex_t *self) { self->current->len++; }

static void finish_operator_composition(lex_t *self, int c) {
    (void)c;
    token_t *token = self->current;
    token->len++;
    assert(c == '=');
    char first = self->source[token->offset];
    if (first == '=')
        token->type = TEQ;
    else if (first == '!')
        token->type = TNE;
    else if (first == '<')
        token->type = TLE;
    else if (first == '>')
        token->type = TGE;
    else
        assert(false);
    finish_token(self, token);
}

static void finish_keyword_composition(lex_t *self, token_t *token) {
    char const *w = self->source + token->offset;
    for (size_t i = 0; i < KEYWORDS_COUNT; i++) {
        char const *kw = keywords[i].keyword;
        size_t kwlen = keywords[i].len;
        if (kwlen == token->len && memcmp(w, kw, kwlen) == 0) {
            token->type = keywords[i].type;
            finish_token(self, token);
            return;
        }
    }
    assert(false);
}

static token_t const *lex_error(lex_t *self, token_t *token) {
    if (self->err_last)
        self->err_last->next = token;
    else
        self->err_first = token;
    self->err_last = token;
    if (!self->pending_err_first) {
        self->pending_err_first = token;
    }
    self->has_err = true;
    return self->pending_first;
}

static token_t const *lex_error_current(lex_t *self) {
    assert(self->current);
    return lex_error(self, self->current);
}

static token_t const *lex_error_new(lex_t *self, int c) {
    (void)c;
    return lex_error(self, new_token(self->offset, TSTRAY));
}

static token_t const *lex_handle(lex_t *self, int c) {
    switch (self->state) {
    case LIDLE:
        if (c == '/') {
            self->state = LCOMMENTBEGIN;
        } else if (is_multichar_beginning(c)) {
            begin_token(self, c);
            self->state = LMULTICHAR;
        } else if (is_single_char(c)) {
            add_token(self, token_type(c));
        } else if (is_number(c)) {
            begin_token(self, c);
            self->state = LNUMBER;
        } else if (is_alpha(c)) {
            begin_token(self, c);
            self->state = LIDENTIFIER;
        } else if (c == '"') {
            begin_token(self, c);
            self->state = LSTRING;
        } else if (!is_space(c) && c != EOF) {
            return lex_error_new(self, c);
        }
        break;
    case LCOMMENTBEGIN:
        if (c == '/') {
            self->state = LCOMMENT;
        } else {
            // Previous byte '/' is a freestanding token, add it
            self->offset--;
            add_token(self, token_type(self->source[self->offset]));
            self->offset++;
            // Forward the iteration execution to LIDLE state
            self->state = LIDLE;
            return lex_handle(self, c);
        }
        break;
    case LCOMMENT:
        if (c == '\n' || c == EOF) {
            self->state = LIDLE;
        }
        break;
    case LSTRING:
        continue_token(self);
        if (c == '"') {
            finish_token(self, self->current);
            self->state = LIDLE;
        } else if (c == '\\') {
            self->state = LSTRINGESCAPE;
        } else if (c == EOF) {
            return lex_error_current(self);
        }
        // XXX: I don't know how to handle errors in the string literal, so
        // every single possible byte is allowed by now.
        break;
    case LSTRINGESCAPE:
        continue_token(self);
        self->state = LSTRING;
        if (c == EOF) {
            return lex_error_current(self);
        }
        break;
    case LMULTICHAR:
        if (is_operator_composition(self->source, self->current, c)) {
            finish_operator_composition(self, c);
            self->state = LIDLE;
        } else {
            assert(is_single_char(self->source[self->current->offset]));
            finish_token(self, self->current);
            // Forward the iteration execution to LIDLE state
            self->state = LIDLE;
            return lex_handle(self, c);
        }
        break;
    case LIDENTIFIER:
        if (is_alpha(c) || is_number(c)) {
            continue_token(self);
        } else {
            if (is_keyword_composition(self->source, self->current)) {
                finish_keyword_composition(self, self->current);
            } else {
                finish_token(self, self->current);
            }
            // Forward the iteration execution to LIDLE state
            self->state = LIDLE;
            return lex_handle(self, c);
        }
        break;
    case LNUMBER:
        if (is_number(c)) {
            continue_token(self);
        } else if (c == '.') {
            continue_token(self);
            self->state = LNUMBERDOTBEGIN;
        } else {
            finish_token(self, self->current);
            // Forward the iteration execution to LIDLE state
            self->state = LIDLE;
            return lex_handle(self, c);
        }
        break;
    case LNUMBERDOTBEGIN:
        if (is_number(c)) {
            continue_token(self);
            self->state = LNUMBERDOT;
        } else {
            return lex_error_current(self);
        }
        break;
    case LNUMBERDOT:
        if (is_number(c)) {
            continue_token(self);
        } else {
            finish_token(self, self->current);
            // Forward the iteration execution to LIDLE state
            self->state = LIDLE;
            return lex_handle(self, c);
        }
        break;
    }
    return self->pending_first;
}

//! Returns pointer to first new token in the linked list
token_t const *lex_consume(lex_t *self, int c) {
    self->source[self->offset] = c;
    token_t const *new = lex_handle(self, c);
    self->offset++;
    CHECK_BUFFER_OFERFLOW(self->offset, BUFFER_SIZE);
    self->pending_first = NULL;
    return new;
}

static const char *token_type_to_string(token_type_t t) {
    switch (t) {
    case TSTRAY:
        return "stray";
    case TCOMMA:
        return "comma";
    case TSEMICOLON:
        return "semicolon";
    case TLBRACE:
        return "lbrace";
    case TRBRACE:
        return "rbrace";
    case TASSIGN:
        return "assign";
    case TOR:
        return "kw-or";
    case TAND:
        return "kw-and";
    case TEQ:
        return "eq";
    case TNE:
        return "ne";
    case TLE:
        return "le";
    case TGE:
        return "ge";
    case TLT:
        return "lt";
    case TGT:
        return "gt";
    case TPLUS:
        return "plus";
    case TMINUS:
        return "minus";
    case TSTAR:
        return "star";
    case TSLASH:
        return "slash";
    case TBANG:
        return "bang";
    case TDOT:
        return "dot";
    case TLPAREN:
        return "lparen";
    case TRPAREN:
        return "rparen";
    case TSTRING:
        return "string";
    case TIDENTIFIER:
        return "identifier";
    case TNUMBER:
        return "number";
    case TIF:
        return "kw-if";
    case TFOR:
        return "kw-for";
    case TFUN:
        return "kw-fun";
    case TNIL:
        return "kw-nil";
    case TVAR:
        return "kw-var";
    case TELSE:
        return "kw-else";
    case TTHIS:
        return "kw-this";
    case TTRUE:
        return "kw-true";
    case TCLASS:
        return "kw-class";
    case TFALSE:
        return "kw-false";
    case TPRINT:
        return "kw-print";
    case TSUPER:
        return "kw-super";
    case TWHILE:
        return "kw-while";
    case TRETURN:
        return "kw-return";
    }
    assert(false);
    return "?";
}

static const char *byte_printable(char c) {
    const char *printable_ascii[256] = {
        "<0x00>", "<0x01>", "<0x02>", "<0x03>", "<0x04>", "<0x05>", "<0x06>",
        "<0x07>", "<0x08>", "<0x09>", "<0x0A>", "<0x0B>", "<0x0C>", "<0x0D>",
        "<0x0E>", "<0x0F>", "<0x10>", "<0x11>", "<0x12>", "<0x13>", "<0x14>",
        "<0x15>", "<0x16>", "<0x17>", "<0x18>", "<0x19>", "<0x1A>", "<0x1B>",
        "<0x1C>", "<0x1D>", "<0x1E>", "<0x1F>", " ",      "!",      "\"",
        "#",      "$",      "%",      "&",      "'",      "(",      ")",
        "*",      "+",      ",",      "-",      ".",      "/",      "0",
        "1",      "2",      "3",      "4",      "5",      "6",      "7",
        "8",      "9",      ":",      ";",      "<",      "=",      ">",
        "?",      "@",      "A",      "B",      "C",      "D",      "E",
        "F",      "G",      "H",      "I",      "J",      "K",      "L",
        "M",      "N",      "O",      "P",      "Q",      "R",      "S",
        "T",      "U",      "V",      "W",      "X",      "Y",      "Z",
        "[",      "\\",     "]",      "^",      "_",      "<`>",    "a",
        "b",      "c",      "d",      "e",      "f",      "g",      "h",
        "i",      "j",      "k",      "l",      "m",      "n",      "o",
        "p",      "q",      "r",      "s",      "t",      "u",      "v",
        "w",      "x",      "y",      "z",      "{",      "|",      "}",
        "~",      "<0x7F>", "<0x80>", "<0x81>", "<0x82>", "<0x83>", "<0x84>",
        "<0x85>", "<0x86>", "<0x87>", "<0x88>", "<0x89>", "<0x8A>", "<0x8B>",
        "<0x8C>", "<0x8D>", "<0x8E>", "<0x8F>", "<0x90>", "<0x91>", "<0x92>",
        "<0x93>", "<0x94>", "<0x95>", "<0x96>", "<0x97>", "<0x98>", "<0x99>",
        "<0x9A>", "<0x9B>", "<0x9C>", "<0x9D>", "<0x9E>", "<0x9F>", "<0xA0>",
        "<0xA1>", "<0xA2>", "<0xA3>", "<0xA4>", "<0xA5>", "<0xA6>", "<0xA7>",
        "<0xA8>", "<0xA9>", "<0xAA>", "<0xAB>", "<0xAC>", "<0xAD>", "<0xAE>",
        "<0xAF>", "<0xB0>", "<0xB1>", "<0xB2>", "<0xB3>", "<0xB4>", "<0xB5>",
        "<0xB6>", "<0xB7>", "<0xB8>", "<0xB9>", "<0xBA>", "<0xBB>", "<0xBC>",
        "<0xBD>", "<0xBE>", "<0xBF>", "<0xC0>", "<0xC1>", "<0xC2>", "<0xC3>",
        "<0xC4>", "<0xC5>", "<0xC6>", "<0xC7>", "<0xC8>", "<0xC9>", "<0xCA>",
        "<0xCB>", "<0xCC>", "<0xCD>", "<0xCE>", "<0xCF>", "<0xD0>", "<0xD1>",
        "<0xD2>", "<0xD3>", "<0xD4>", "<0xD5>", "<0xD6>", "<0xD7>", "<0xD8>",
        "<0xD9>", "<0xDA>", "<0xDB>", "<0xDC>", "<0xDD>", "<0xDE>", "<0xDF>",
        "<0xE0>", "<0xE1>", "<0xE2>", "<0xE3>", "<0xE4>", "<0xE5>", "<0xE6>",
        "<0xE7>", "<0xE8>", "<0xE9>", "<0xEA>", "<0xEB>", "<0xEC>", "<0xED>",
        "<0xEE>", "<0xEF>", "<0xF0>", "<0xF1>", "<0xF2>", "<0xF3>", "<0xF4>",
        "<0xF5>", "<0xF6>", "<0xF7>", "<0xF8>", "<0xF9>", "<0xFA>", "<0xFB>",
        "<0xFC>", "<0xFD>", "<0xFE>", "<0xFF>",
    };
    return printable_ascii[(size_t)(c & 0xFF)];
}

static void fprint_escaped(FILE *stream, char const *string, size_t len) {
    for (size_t i = 0; i < len; i++) {
        fputs(byte_printable(string[i]), stream);
    }
}

void print_tokens(char const *source, token_t const *token) {
    if (!token)
        return;

    printf("info: lex: [");
    while (token) {
        size_t maxlen = MAX_TOKEN_LEN_PRINT;
        int len = token->len > maxlen ? maxlen : token->len;
        fprintf(stdout, "%s<", token_type_to_string(token->type));
        fprint_escaped(stdout, source + token->offset, len);
        fprintf(
            stdout,
            "%s>%s",
            token->len > maxlen ? "..." : "",
            token->next ? ", " : "");
        token = token->next;
    }
    printf("]\n");
}

static void offset_to_line_and_col(
    char const *source,
    size_t offset,
    unsigned long *line,
    unsigned long *col) {
    assert(line);
    assert(col);
    *line = *col = 1;
    for (size_t i = 0; i < offset; i++) {
        if (source[i] == '\n') {
            *col = 1;
            ++*line;
        } else {
            ++*col;
        }
    }
}

static void lex_print_error(lex_t const *self, token_t const *token) {
    unsigned long line = 1, col = 1;
    offset_to_line_and_col(self->source, token->offset, &line, &col);
    size_t maxlen = MAX_TOKEN_LEN_PRINT;
    int len = token->len > maxlen ? maxlen : token->len;
    fprintf(
        stderr,
        "error: lex: <stdin>:%lu:%lu: invalid token `%s<%.*s%s>`\n",
        line,
        col,
        token_type_to_string(token->type),
        len,
        self->source + token->offset,
        token->len > maxlen ? "..." : "");
}

void lex_report_errors_if_any(lex_t *self) {
    if (!self->has_err)
        return;

    while (self->pending_err_first) {
        lex_print_error(self, self->pending_err_first);
        self->pending_err_first = self->pending_err_first->next;
    }
    self->has_err = false;
}

void lex_deinit(lex_t *self) {
    token_t *token = self->first;
    while (token) {
        token_t *next = token->next;
        free(token);
        token = next;
    }
    token_t *err_token = self->err_first;
    while (err_token) {
        token_t *next = err_token->next;
        free(err_token);
        err_token = next;
    }
}

expression_t *new_expression(exprtype_t type) {
    expression_t *expr = calloc(1, sizeof(expression_t));
    CHECK_ALLOC(expr);
    *expr = (expression_t){
        .type = type,
    };
    return expr;
}

expression_t *new_expr_anon(expression_t *nested) {
    expression_t *expr = new_expression(EXANON);
    expr->anon.value = nested;
    return expr;
}

expression_t *new_expr_unary(token_type_t token_type) {
    expression_t *expr = new_expression(EXUNARY);
    expr->unary = (struct expr_unary){
        .type = token_type == TBANG ? UNARY_INV : UNARY_NEG,
    };
    return expr;
}

static exprtype_t token_to_expr_type(token_type_t token_type) {
    switch (token_type) {
    case TOR:
        return EXOR;
    case TAND:
        return EXAND;
    case TEQ:
    case TNE:
        return EXEQUAL;
    case TLE:
    case TGE:
    case TLT:
    case TGT:
        return EXCOMPAR;
    case TPLUS:
    case TMINUS:
        return EXTERM;
    case TSTAR:
    case TSLASH:
        return EXFACTOR;
    case TSTRING:
        return EXSTRING;
    case TIDENTIFIER:
        return EXIDENTIFIER;
    case TNUMBER:
        return EXNUMBER;
    case TNIL:
        return EXNIL;
    case TTHIS:
        return EXTHIS;
    case TTRUE:
        return EXTRUE;
    case TFALSE:
        return EXFALSE;
    default:
        break;
    }
    assert(false);
    return 0;
}

expression_t *new_expr_assign(void) { return new_expression(EXASSIGN); }

expression_t *new_expr_binary(exprtype_t exprtype, binexprtype_t bintype) {
    expression_t *expr = new_expression(exprtype);
    expr->binary.type = bintype;
    return expr;
}

expression_t *new_expr_call(call_type_t type) {
    expression_t *expr = new_expression(EXCALL);
    expr->call.type = type;
    return expr;
}

expression_t *new_expr_literal(token_t const *token) {
    expression_t *expr = new_expression(token_to_expr_type(token->type));
    expr->literal = *token;
    return expr;
}

expression_t *new_expr_super(token_t const *token) {
    expression_t *expr = new_expression(EXSUPER);
    expr->literal = *token;
    return expr;
}

pars_stack_frame_t *new_pars_stack_frame(expression_t *expr) {
    pars_stack_frame_t *frame = calloc(1, sizeof(pars_stack_frame_t));
    CHECK_ALLOC(frame);
    *frame = (pars_stack_frame_t){
        .expr = expr,
    };
    return frame;
}

void pars_init(pars_t *self, char const *source) {
    *self = (pars_t){
        .source = source,
        .stack = new_pars_stack_frame(NULL),
    };
}

static void destroy_expression(expression_t *expression) {
    if (!expression)
        return;
    switch (expression->type) {
    case EXMIN:
        break;
    case EXASSIGN:
        destroy_expression(expression->assign.to);
        destroy_expression(expression->assign.from);
        break;
    case EXOR:
    case EXAND:
    case EXEQUAL:
    case EXCOMPAR:
    case EXTERM:
    case EXFACTOR:
        destroy_expression(expression->binary.left);
        destroy_expression(expression->binary.right);
        break;
    case EXUNARY:
        destroy_expression(expression->unary.value);
        break;
    case EXCALL:
        destroy_expression(expression->call.callee);
        if (expression->call.first) {
            struct call_arg *arg = expression->call.first;
            while (arg) {
                destroy_expression(arg->expr);
                struct call_arg *next = arg->next;
                free(arg);
                arg = next;
            }
        }
        break;
    case EXANON:
        if (expression->anon.value)
            destroy_expression(expression->anon.value);
        break;
    case EXTRUE:
    case EXFALSE:
    case EXNIL:
    case EXTHIS:
    case EXNUMBER:
    case EXSTRING:
    case EXIDENTIFIER:
    case EXSUPER:
    case EXMAX:
        break;
    }
    free(expression);
}

static void destroy_statement(statement_t *statement) {
    switch (statement->type) {
    case STEXPR:
        destroy_expression(statement->expr);
        break;
    }
    free(statement);
}

static statement_t *new_statement(stmttype_t type) {
    statement_t *stmt = calloc(1, sizeof(statement_t));
    CHECK_ALLOC(stmt);
    *stmt = (statement_t){
        .type = type,
    };
    return stmt;
}

static statement_t *new_stmt_expr(expression_t *expr) {
    statement_t *stmt = new_statement(STEXPR);
    stmt->expr = expr;
    return stmt;
}

statement_t *pars_error(pars_t *self, token_t const *token) {
    self->has_err = true;
    self->err_token = copy_token(token);
    pars_stack_frame_t *frame = self->stack;
    if (!frame)
        return NULL;
    // Destroy the whole stack except the last frame
    while (frame->next) {
        destroy_expression(frame->expr);
        pars_stack_frame_t *next = frame->next;
        free(frame);
        frame = next;
    }
    frame->state = PEXPR;
    self->stack = frame;
    expression_t *expr = frame->expr;
    frame->expr = NULL;
    return new_stmt_expr(expr);
}

static bool is_literal(token_type_t token_type) {
    return token_type >= TSTRING || token_type == TOR || token_type == TAND;
}

static bool is_callable(expression_t *expr) {
    if (!expr)
        return false;
    return expr->type >= EXCALL;
}

static bool is_assignable(expression_t *expr) {
    if (!expr)
        return false;
    return (expr->type == EXCALL && expr->call.type == CALL_ID) ||
           expr->type == EXIDENTIFIER;
}

static bool is_inversion(token_type_t token_type) {
    return token_type == TBANG || token_type == TMINUS;
}

static bool is_binary_op(token_type_t token_type) {
    return token_type >= TOR && token_type <= TSLASH;
}

static binexprtype_t token_to_binexpr_type(token_type_t token_type) {
    switch (token_type) {
    case TOR:
        return BINARY_OR;
    case TAND:
        return BINARY_AND;
    case TEQ:
        return BINARY_EQ;
    case TNE:
        return BINARY_NE;
    case TLE:
        return BINARY_LE;
    case TGE:
        return BINARY_GE;
    case TLT:
        return BINARY_LT;
    case TGT:
        return BINARY_GT;
    case TPLUS:
        return BINARY_SUM;
    case TMINUS:
        return BINARY_SUB;
    case TSTAR:
        return BINARY_MUL;
    case TSLASH:
        return BINARY_DIV;
    default:
        break;
    }
    assert(false);
    return 0;
}

static struct call_arg *new_call_arg(expression_t *expr) {
    struct call_arg *arg = calloc(1, sizeof(struct call_arg));
    arg->expr = expr;
    return arg;
}

static expression_t *pars_pop(pars_t *self) {
    pars_stack_frame_t *frame = self->stack;
    expression_t *expr = frame->expr;
    self->stack = frame->next;
    free(frame);
    return expr;
}

static void pars_unwind(pars_t *self, exprtype_t precedence) {
    pars_stack_frame_t *next = self->stack->next;
    while (next && next->expr && next->expr->type > precedence) {
        if (next->expr->type >= EXCALL)
            break;
        if (next->expr->type == EXUNARY) {
            next->expr->unary.value = pars_pop(self);
            next = self->stack->next;
        } else if (next->expr->type <= EXFACTOR && next->expr->type >= EXOR) {
            next->expr->binary.right = pars_pop(self);
            next = self->stack->next;
        } else if (next->expr->type == EXASSIGN) {
            next->expr->assign.from = pars_pop(self);
            next = self->stack->next;
        } else {
            // TODO Implement
            assert(false);
            break;
        }
    }
}

static void add_call_arg(struct expr_call *call, expression_t *arg_ex) {
    struct call_arg *arg = new_call_arg(arg_ex);
    if (call->last) {
        call->last->next = arg;
    } else {
        call->first = arg;
    }
    call->last = arg;
}

static void pars_push(pars_t *self) {
    pars_stack_frame_t *next = self->stack;
    self->stack = new_pars_stack_frame(NULL);
    self->stack->next = next;
}

static void add_expr(pars_t *self, expression_t *expr) {
    pars_stack_frame_t *stack = self->stack;
    if (stack->expr) {
        expression_t* sexpr = stack->expr;
        if (expr->type == EXCALL) {
            assert(sexpr);
            expr->call.callee = sexpr;
            stack->expr = expr;
        } else if (expr->type <= EXFACTOR && expr->type >= EXOR) {
            assert(sexpr);
            expr->binary.left = sexpr;
            stack->expr = expr;
        } else if (expr->type == EXASSIGN) {
            assert(sexpr);
            expr->assign.to = sexpr;
            stack->expr = expr;
        } else if (sexpr->type == EXUNARY) {
            sexpr->unary.value = expr;
        } else if (sexpr->type <= EXFACTOR && sexpr->type >= EXOR) {
            pars_push(self);
            stack->expr = expr;
        } else {
            assert(false);
        }
    } else {
        stack->expr = expr;
    }
}

statement_t *pars_consume(pars_t *self, token_t const *token, expression_t* nested) {
    switch (self->stack->state) {
    case PEXPR:
        if (token->type == TLPAREN) {
            self->stack->state = PANONEXPR;
            add_expr(self, new_expr_anon(NULL));
            pars_push(self);
        } else if (token->type == TSUPER) {
            self->stack->state = PSUPERDOT;
        } else if (is_literal(token->type)) {
            add_expr(self, new_expr_literal(token));
            self->stack->state = POPER;
        } else if (is_inversion(token->type)) {
            add_expr(self, new_expr_unary(token->type));
            pars_push(self);
        } else if (token->type == TSEMICOLON && !self->stack->next) {
            expression_t *expr = self->stack->expr;
            self->stack->expr = NULL;
            return new_stmt_expr(expr);
        } else {
            return pars_error(self, token);
        }
        break;
    case PANONEXPR:
        if (token->type == TRPAREN) {
            self->stack->expr->anon.value = nested;
            self->stack->state = POPER;
        } else {
            return pars_error(self, token);
        }
        break;
    case POPER:
        if (token->type == TRPAREN) {
            pars_unwind(self, EXMIN);
            if (!self->stack->next)
                return pars_error(self, token);
            // Forward handling to underlying PANONEXPR or PCALLARGDELIM state
            return pars_consume(self, token, pars_pop(self));
        } else if (token->type == TCOMMA) {
            pars_unwind(self, EXMIN);
            if (!self->stack->next)
                return pars_error(self, token);
            // Forward handling to underlying PCALLARGDELIM state
            return pars_consume(self, token, pars_pop(self));
        } else if (token->type == TLPAREN) {
            if (is_callable(self->stack->expr)) {
                add_expr(self, new_expr_call(CALL_CALL));
                self->stack->state = PCALLARG;
            } else {
                return pars_error(self, token);
            }
            self->stack->state = PCALLARG;
        } else if (token->type == TDOT) {
            if (is_callable(self->stack->expr)) {
                add_expr(self, new_expr_call(CALL_ID));
                self->stack->state = PCALLID;
            } else {
                return pars_error(self, token);
            }
        } else if (token->type == TASSIGN) {
            if (is_assignable(self->stack->expr)) {
                pars_unwind(self, EXASSIGN);
                add_expr(self, new_expr_assign());
                pars_push(self);
                self->stack->state = PEXPR;
            } else {
                return pars_error(self, token);
            }
        } else if (token->type == TSEMICOLON) {
            pars_unwind(self, EXMIN);
            if (self->stack->next)
                return pars_error(self, token);
            expression_t *expr = self->stack->expr;
            self->stack->expr = NULL;
            self->stack->state = PEXPR;
            return new_stmt_expr(expr);
        } else if (is_binary_op(token->type)) {
            exprtype_t exprtype = token_to_expr_type(token->type);
            binexprtype_t binexprtype = token_to_binexpr_type(token->type);
            pars_unwind(self, exprtype);
            add_expr(self, new_expr_binary(exprtype, binexprtype));
            pars_push(self);
            self->stack->state = PEXPR;
        } else {
            return pars_error(self, token);
        }
        break;
    case PCALLID:
        if (token->type == TIDENTIFIER) {
            self->stack->expr->call.id = *token;
            self->stack->state = POPER;
        } else {
            return pars_error(self, token);
        }
        break;
    case PCALLARG: {
        bool has_first = self->stack->expr->call.first;
        if (token->type == TRPAREN && !has_first) {
            self->stack->state = POPER;
        } else {
            self->stack->state = PCALLARGDELIM;
            pars_push(self);
            // Forward handling to PEXPR state
            self->stack->state = PEXPR;
            return pars_consume(self, token, NULL);
        }
        break;
    }
    case PCALLARGDELIM:
        if (token->type == TRPAREN) {
            add_call_arg(&self->stack->expr->call, nested);
            self->stack->state = POPER;
        } else if (token->type == TCOMMA) {
            add_call_arg(&self->stack->expr->call, nested);
            self->stack->state = PCALLARG;
        } else {
            return pars_error(self, token);
        }
        break;
    case PSUPERDOT:
        if (token->type == TDOT) {
            self->stack->state = PSUPERID;
        } else {
            return pars_error(self, token);
        }
        break;
    case PSUPERID:
        if (token->type == TIDENTIFIER) {
            add_expr(self, new_expr_super(token));
            self->stack->state = POPER;
        } else {
            return pars_error(self, token);
        }
        break;
    }
    return NULL;
}

void pars_report_errors_if_any(pars_t *self) {
    if (!self->has_err)
        return;
    token_t *token = self->err_token;
    unsigned long line = 1, col = 1;
    offset_to_line_and_col(self->source, token->offset, &line, &col);
    size_t maxlen = MAX_TOKEN_LEN_PRINT;
    int len = token->len > maxlen ? maxlen : token->len;
    fprintf(
        stderr,
        "error: pars: <stdin>:%lu:%lu: unexpected token `%s<%.*s%s>`\n",
        line,
        col,
        token_type_to_string(token->type),
        len,
        self->source + token->offset,
        token->len > maxlen ? "..." : "");
    free(token);
    self->err_token = NULL;
    self->has_err = false;
}

static void fprint_literal(FILE *fd, char const *source, token_t const *token) {
    size_t maxlen = MAX_TOKEN_LEN_PRINT;
    int len = token->len > maxlen ? maxlen : token->len;
    fprint_escaped(fd, source + token->offset, len);
}

const char *binary_op_printable(binexprtype_t type) {
    switch (type) {
    case BINARY_OR:
        return "or";
    case BINARY_AND:
        return "and";
    case BINARY_EQ:
        return "==";
    case BINARY_NE:
        return "!=";
    case BINARY_LE:
        return "<=";
    case BINARY_GE:
        return ">=";
    case BINARY_LT:
        return "<";
    case BINARY_GT:
        return ">";
    case BINARY_SUM:
        return "+";
    case BINARY_SUB:
        return "-";
    case BINARY_MUL:
        return "*";
    case BINARY_DIV:
        return "/";
    }
    return "<invalid>";
}

void print_expression(char const *source, expression_t const *expression) {
    if (!expression)
        return;
    switch (expression->type) {
    case EXMIN:
        fprintf(stdout, "<NOT-IMPLEMENTED>");
        break;
    case EXASSIGN:
        print_expression(source, expression->assign.to);
        fprintf(stdout, " = ");
        print_expression(source, expression->assign.from);
        break;
    case EXOR:
    case EXAND:
    case EXEQUAL:
    case EXCOMPAR:
    case EXTERM:
    case EXFACTOR:
        print_expression(source, expression->binary.left);
        fprintf(stdout, " %s ", binary_op_printable(expression->binary.type));
        print_expression(source, expression->binary.right);
        break;
    case EXUNARY:
        fprintf(stdout, "%s", expression->unary.type == UNARY_INV ? "!" : "-");
        if (expression->unary.value)
            print_expression(source, expression->unary.value);
        else
            fprintf(stdout, "<empty>");
        break;
    case EXCALL:
        print_expression(source, expression->call.callee);
        if (expression->call.type == CALL_ID) {
            fprintf(stdout, ".");
            fprint_literal(stdout, source, &expression->call.id);
        } else {
            fprintf(stdout, "(");
            struct call_arg *arg = expression->call.first;
            while (arg) {
                print_expression(source, arg->expr);
                fprintf(stdout, "%s", arg->next ? ", " : "");
                arg = arg->next;
            }
            fprintf(stdout, ")");
        }
        break;
    case EXANON:
        fprintf(stdout, "(");
        print_expression(source, expression->anon.value);
        fprintf(stdout, ")");
        break;
    case EXTRUE:
    case EXFALSE:
    case EXNIL:
    case EXTHIS:
    case EXNUMBER:
    case EXSTRING:
    case EXIDENTIFIER:
        fprint_literal(stdout, source, &expression->literal);
        break;
    case EXSUPER:
        fprintf(stdout, "super.");
        fprint_literal(stdout, source, &expression->literal);
        break;
    case EXMAX:
        break;
    }
}

void print_expression_statement(
    char const *source, expression_t const *expression) {
    fprintf(stdout, "info: expr-stmt: ");
    print_expression(source, expression);
    fprintf(stdout, ";\n");
}

void print_statement(char const *source, statement_t const *statement) {
    switch (statement->type) {
    case STEXPR:
        print_expression_statement(source, statement->expr);
        break;
    }
}

void pars_deinit(pars_t *self) {
    pars_stack_frame_t *frame = self->stack;
    while (frame) {
        destroy_expression(frame->expr);
        pars_stack_frame_t *next = frame->next;
        free(frame);
        frame = next;
    }
    token_t *err_token = self->err_token;
    while (err_token) {
        token_t *next = err_token->next;
        free(err_token);
        err_token = next;
    }
}

void interp_init(interp_t *self, char const *source) {
    *self = (interp_t){
        .source = source,
    };
}

static void interp_print_result(interp_t *self) {
    (void)self;
    ;
}

status_t interp_exec(interp_t *self, expression_t const *expr) {
    (void)expr;
    interp_print_result(self);
    return STATUS_OK;
}

void interp_print_error(interp_t *self) {
    (void)self;
    ;
}

void interp_deinit(interp_t *self) {
    (void)self;
    ;
}

int main() {
    char *source = calloc(1, BUFFER_SIZE);
    CHECK_ALLOC(source);
    lex_t lex;
    pars_t pars;
    interp_t interp;
    lex_init(&lex, source);
    pars_init(&pars, source);
    interp_init(&interp, source);
    char c;
    do {
        c = getchar();
        token_t const *token = lex_consume(&lex, c);
        lex_report_errors_if_any(&lex);
        if (0)
            print_tokens(source, token);
        while (token) {
            statement_t *statement = pars_consume(&pars, token, NULL);
            pars_report_errors_if_any(&pars);
            if (statement) {
                print_statement(source, statement);
                destroy_statement(statement);
            }
            token = token->next;
        }
    } while (c != EOF);
    interp_deinit(&interp);
    pars_deinit(&pars);
    lex_deinit(&lex);
    free(source);
}
