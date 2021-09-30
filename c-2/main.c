#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    TLITERAL,
    TIDENTIFIER,
    TLBRACE,
    TRBRACE,
    TLBRACKET,
    TRBRACKET,
    TLPAREN,
    TRPAREN,
    TEQ,
    TDOT,
    TPIPE,
} token_type_t;

typedef enum {
    LIDLE,       // Skipping spaces, taking single-char tokens
    LIDENTIFIER, // Filling identifier token
    LLITERAL,    // Filling literal token
    LLITQUOTE,   // Filling literal token, awaiting second "
} lex_state_t;

typedef struct token_s {
    struct token_s *next;
    token_type_t type;
    unsigned long line;
    unsigned long col;
    size_t offset;
    size_t len;
} token_t;

typedef struct {
    token_t *first;
    token_t *last;
    char *source;
    lex_state_t state;
    bool has_error;
    unsigned long line;
    unsigned long col;
    size_t offset;
} lex_t;

typedef enum {
    PRULE,   // Awaiting rule identifier
    PEQ,     // Awaiting =
    PTERM,   // First factor in term
    PFACTOR, // Subsequent factors in term
} parse_state_t;

typedef enum {
    FIDENTIFIER,
    FLITERAL,
    FEXPRESSION,
} factor_type_t;

struct expression_s;

typedef struct factor_s {
    struct factor_s *next;
    struct expression_s *expr;
    factor_type_t type;
    unsigned long line;
    unsigned long col;
    size_t offset;
    size_t len;
} factor_t;

typedef struct term_s {
    struct term_s *next;
    factor_t *first;
    factor_t *last;
} term_t;

typedef enum {
    ERULE,
    EOPTIONAL,
    EREPETITION,
    EGROUP,
} expression_type_t;

typedef struct expression_s {
    struct expression_s *back;
    term_t *first;
    term_t *last;
    expression_type_t type;
} expression_t;

typedef struct rule_s {
    struct rule_s *next;
    factor_t *name;
    expression_t *expr;
} rule_t;

typedef enum {
    PSOK = 0,
    PSEOF,
    PSINVALIDTOKEN,
} pars_status_t;

typedef struct {
    parse_state_t state;
    rule_t *first;
    rule_t *last;
    expression_t *stack;
    bool has_error;
    token_t err_token;
    pars_status_t status;
    const char *source;
} pars_t;

static bool is_character(int c) { return c >= 0x20 && c <= 0x7E; }

static bool is_letter(int c) {
    return (c >= 0x30 && c <= 0x39) || (c >= 0x41 && c <= 0x5A) ||
           (c >= 0x61 && c <= 0x7A) || c == '-' || c == '_';
}

static bool is_single_char_token(int c) {
    return c == '{' || c == '}' || c == '[' || c == ']' || c == '(' ||
           c == ')' || c == '|' || c == '=' || c == '.';
}

static bool is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void lex_init(lex_t *self) {
    *self = (lex_t){
        .state = LIDLE,
        .source = calloc(1024*100, 1),
        .line = 1,
        .col = 1,
    };
    assert(self->source);
}

static token_type_t token_type(int c) {
    switch (c) {
    case '{':
        return TLBRACE;
    case '}':
        return TRBRACE;
    case '[':
        return TLBRACKET;
    case ']':
        return TRBRACKET;
    case '(':
        return TLPAREN;
    case ')':
        return TRPAREN;
    case '|':
        return TPIPE;
    case '=':
        return TEQ;
    case '.':
        return TDOT;
    case '"':
        return TLITERAL;
    default:
        if (is_letter(c))
            return TIDENTIFIER;
        assert(false);
    };
    return 0;
};

static void add_token(lex_t *self, int c) {
    token_t *token = calloc(1, sizeof(token_t));
    *token = (token_t){
        .type = token_type(c),
        .line = self->line,
        .col = self->col,
        .offset = self->offset,
        .len = 1,
    };
    if (self->last)
        self->last->next = token;
    else
        self->first = token;
    self->last = token;
}

static void continue_token(lex_t *self) {
    self->last->col++;
    self->last->len++;
}

static bool lex_error(lex_t *self, int c) {
    self->source[self->offset] = c;
    self->has_error = true;
    return !self->has_error;
}

bool lex_consume(lex_t *self, int c) {
    if (self->has_error)
        return false;
    switch (self->state) {
    case LIDLE:
        if (is_letter(c)) {
            add_token(self, c);
            self->state = LIDENTIFIER;
        } else if (c == '"') {
            add_token(self, c);
            self->state = LLITERAL;
        } else if (is_single_char_token(c)) {
            add_token(self, c);
        } else if (is_space(c)) {
            // Skip
        } else {
            return lex_error(self, c);
        }
        break;
    case LIDENTIFIER:
        if (is_letter(c)) {
            continue_token(self);
        } else if (c == '"') {
            add_token(self, c);
            self->state = LLITERAL;
        } else if (is_single_char_token(c)) {
            add_token(self, c);
            self->state = LIDLE;
        } else if (is_space(c)) {
            self->state = LIDLE;
        } else {
            return lex_error(self, c);
        }
        break;
    case LLITERAL:
        if (c == '"') {
            continue_token(self);
            self->state = LLITQUOTE;
        } else if (is_character(c)) {
            continue_token(self);
        } else {
            return lex_error(self, c);
        }
        break;
    case LLITQUOTE:
        if (c == '"') {
            continue_token(self);
            self->state = LLITERAL;
        } else if (is_letter(c)) {
            add_token(self, c);
            self->state = LIDENTIFIER;
        } else if (c == '"') {
            add_token(self, c);
            self->state = LLITERAL;
        } else if (is_single_char_token(c)) {
            add_token(self, c);
            self->state = LIDLE;
        } else if (is_space(c)) {
            self->state = LIDLE;
        } else {
            return lex_error(self, c);
        }
        break;
    }
    self->source[self->offset] = c;
    self->offset++;
    if (c == '\n') {
        self->line++;
        self->col = 1;
    } else {
        self->col++;
    }
    return !self->has_error;
}

static const char *token_type_to_string(token_type_t t) {
    switch (t) {
    case TLITERAL:
        return "literal";
    case TIDENTIFIER:
        return "id";
    case TLBRACE:
        return "lbrace";
    case TRBRACE:
        return "rbrace";
    case TLBRACKET:
        return "lbracket";
    case TRBRACKET:
        return "rbracket";
    case TLPAREN:
        return "lparen";
    case TRPAREN:
        return "rparen";
    case TEQ:
        return "eq";
    case TDOT:
        return "dot";
    case TPIPE:
        return "pipe";
    }
    assert(false);
    return "?";
}

void lex_print(lex_t const *self) {
    printf("[");
    token_t *token = self->first;
    while (token) {
        int len = token->len > 32 ? 32 : token->len;
        printf(
            "%s<%.*s>%s",
            token_type_to_string(token->type),
            len,
            self->source + token->offset,
            token->next ? ", " : "");
        token = token->next;
    }
    printf("]\n");
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

static const char *lex_expected(lex_t const *self) {
    switch (self->state) {
    case LLITQUOTE:
    case LLITERAL:
        return "`[ ~!@#$%^&*()_+={}/\\|,.<>?'\"a-zA-Z0-9]`, `[`, `]`, `-` "
               "or `<`>`";
    case LIDLE:
        return "`[_a-zA-Z0-9(){}[]=.|]`, `-`, or `\"`";
    case LIDENTIFIER:
        return "`[_a-zA-Z0-9]` or `-`";
    }
    assert(false);
    return "???";
}

void lex_print_err(lex_t const *self) {
    fprintf(
        stderr,
        "error lexing <stdin>:%lu:%lu: unexpected token `%s`, "
        "expected %s\n",
        self->line,
        self->col,
        byte_printable(self->source[self->offset]),
        lex_expected(self));
}

void lex_deinit(lex_t *self) {
    token_t *token = self->first;
    while (token) {
        token_t *next = token->next;
        free(token);
        token = next;
    }
    free(self->source);
}

void pars_init(pars_t *self) {
    *self = (pars_t){
        .state = PRULE,
        .err_token =
            {
                .line = 1,
                .col = 1,
            },
    };
}

static pars_status_t pars_error(pars_t *self, token_t *token) {
    self->has_error = true;
    self->err_token = *token;
    return self->status = PSINVALIDTOKEN;
}

static void add_rule(pars_t *self, token_t *token) {
    factor_t *name = calloc(1, sizeof(factor_t));
    assert(name);
    *name = (factor_t){
        .type = FIDENTIFIER,
        .line = token->line,
        .col = token->col,
        .offset = token->offset,
        .len = token->len,
    };
    rule_t *rule = calloc(1, sizeof(rule_t));
    assert(rule);
    rule->name = name;
    if (self->last) {
        self->last->next = rule;
    } else {
        self->first = rule;
    }
    self->last = rule;
}

static inline bool is_open_expression(token_type_t t) {
    return t == TLBRACE || t == TLBRACKET || t == TLPAREN;
}

static inline factor_type_t factor_type(token_type_t t) {
    if (is_open_expression(t))
        return FEXPRESSION;
    else if (t == TLITERAL)
        return FLITERAL;
    else if (t == TIDENTIFIER)
        return FIDENTIFIER;
    assert(false);
    return 0;
}

static inline factor_t factor_from_token(token_t *token) {
    return (factor_t){
        .type = factor_type(token->type),
        .line = token->line,
        .col = token->col,
        .offset = token->offset,
        .len = token->len,
    };
}

static factor_t *add_factor(pars_t *self, token_t *token) {
    factor_t *factor = calloc(1, sizeof(factor_t));
    *factor = factor_from_token(token);
    assert(factor);
    term_t *term = self->stack->last;
    if (term->last)
        term->last->next = factor;
    else
        term->first = factor;
    term->last = factor;
    return factor;
}

static inline expression_type_t expression_type(token_type_t t) {
    if (t == TEQ)
        return ERULE;
    else if (t == TLBRACE)
        return EREPETITION;
    else if (t == TLBRACKET)
        return EOPTIONAL;
    else if (t == TLPAREN)
        return EGROUP;
    assert(false);
    return 0;
}

static void begin_expression(pars_t *self, token_t *token) {
    expression_t *expr = calloc(1, sizeof(expression_t));
    assert(expr);
    expr->type = expression_type(token->type);
    expr->back = self->stack;
    if (token->type == TEQ) {
        self->last->expr = expr;
    } else {
        term_t *term;
        if (self->stack->last) {
            term = self->stack->last;
        } else {
            term = calloc(1, sizeof(term_t));
            assert(term);
            self->stack->first = term;
            self->stack->last = term;
        }
        factor_t *factor = add_factor(self, token);
        factor->expr = expr;
    }
    self->stack = expr;
}

static void add_term(pars_t *self, token_t *token) {
    term_t *term = calloc(1, sizeof(term_t));
    assert(term);
    if (self->stack->last)
        self->stack->last->next = term;
    else
        self->stack->first = term;
    self->stack->last = term;
    add_factor(self, token);
}

static void finish_expression(pars_t *self, token_t *token) {
    self->stack = self->stack->back;
    (void)token;
}

static bool is_close_expression(token_type_t t, expression_type_t e) {
    return (t == TDOT && e == ERULE) || (t == TRBRACE && e == EREPETITION) ||
           (t == TRBRACKET && e == EOPTIONAL) || (t == TRPAREN && e == EGROUP);
}

pars_status_t pars_parse(pars_t *self, lex_t const *lex) {
    self->source = lex->source;
    token_t *token = lex->first;
    while (token) {
        switch (self->state) {
        case PRULE:
            if (token->type == TIDENTIFIER) {
                add_rule(self, token);
                self->state = PEQ;
            } else {
                return pars_error(self, token);
            }
            break;
        case PEQ:
            if (token->type == TEQ) {
                begin_expression(self, token);
                self->state = PTERM;
            } else {
                return pars_error(self, token);
            }
            break;
        case PTERM:
            if (token->type == TIDENTIFIER || token->type == TLITERAL) {
                add_term(self, token);
                self->state = PFACTOR;
            } else if (is_open_expression(token->type)) {
                begin_expression(self, token);
            } else {
                return pars_error(self, token);
            }
            break;
        case PFACTOR:
            if (token->type == TIDENTIFIER || token->type == TLITERAL) {
                add_factor(self, token);
            } else if (is_open_expression(token->type)) {
                begin_expression(self, token);
                self->state = PTERM;
            } else if (is_close_expression(token->type, self->stack->type)) {
                if (self->stack->type == ERULE)
                    self->state = PRULE;
                finish_expression(self, token);
            } else if (token->type == TPIPE) {
                self->state = PTERM;
            } else {
                return pars_error(self, token);
            }
            break;
        }
        self->err_token = *token;
        token = token->next;
    }
    if (self->state != PRULE) {
        self->has_error = true;
        return self->status = PSEOF;
    }
    return PSOK;
}

static void print_expression(char const *source, expression_t *expr) {
    assert(expr);
    const char *opening = "=", *closing = ".";
    if (expr->type == EREPETITION) {
        opening = "{";
        closing = "}";
    } else if (expr->type == EOPTIONAL) {
        opening = "[";
        closing = "]";
    } else if (expr->type == EGROUP) {
        opening = "(";
        closing = ")";
    }
    printf("%s ", opening);
    term_t *term = expr->first;
    while (term) {
        factor_t *factor = term->first;
        while (factor) {
            if (factor->type == FEXPRESSION) {
                print_expression(source, factor->expr);
            } else {
                int len = factor->len > 32 ? 32 : factor->len;
                printf("%.*s ", len, source + factor->offset);
            }
            factor = factor->next;
        }
        if (term->next)
            printf("| ");
        term = term->next;
    }
    printf("%s ", closing);
}

void pars_print(pars_t const *self) {
    rule_t *rule = self->first;
    while (rule) {
        int len = rule->name->len > 32 ? 32 : rule->name->len;
        printf("%.*s ", len, self->source + rule->name->offset);
        print_expression(self->source, rule->expr);
        printf("\n");
        rule = rule->next;
    }
}

static const char *pars_expected(pars_t const *self) {
    switch (self->state) {
    case PRULE:
        return "identifier";
    case PEQ:
        return "`=`";
    case PTERM:
        return "identifier, literal, `{`, `(` or `[`";
    case PFACTOR:
        if (self->stack->type == ERULE) {
            return "identifier, literal, `{`, `(`, `[`, `|` or `.`";
        } else if (self->stack->type == EREPETITION) {
            return "identifier, literal, `{`, `(`, `[`, `|` or `}";
        } else if (self->stack->type == EOPTIONAL) {
            return "identifier, literal, `{`, `(`, `[`, `|` or `]";
        } else if (self->stack->type == EGROUP) {
            return "identifier, literal, `{`, `(`, `[`, `|` or `)";
        }
    }
    assert(false);
    return "???";
}

void pars_print_err(pars_t const *self) {
    token_t const *token = &self->err_token;
    int len = token->len > 32 ? 32 : token->len;
    if (self->status == PSINVALIDTOKEN) {
        fprintf(
            stderr,
            "error: parsing <stdin>:%lu:%lu: unexpected token `%.*s`, "
            "expected %s\n",
            token->line,
            token->col,
            len,
            self->source + token->offset,
            pars_expected(self));
    } else {
        fprintf(
            stderr,
            "error: parsing <stdin>:%lu:%lu: unexpected EOF",
            token->line,
            token->col + token->len);
    }
}

void pars_deinit(pars_t *self) {
    rule_t *rule = self->first;
    while (rule) {
        expression_t *expr = rule->expr;
        while (expr) {
            while (expr->first) {
                term_t *term = expr->first;
                while (term->first) {
                    factor_t *factor = term->first;
                    if (factor->type == FEXPRESSION && factor->expr) {
                        // Put on the expressions "stack" to free it later
                        // in next interation of `while (expr)` in order to
                        // mitigate recursive calls
                        factor->expr->back = expr->back;
                        expr->back = factor->expr;
                    }
                    factor_t *factor_next = factor->next;
                    free(factor);
                    term->first = factor_next;
                }
                term_t *term_next = term->next;
                free(term);
                expr->first = term_next;
            }
            // Pop the expressions "stack" to free nested expressions
            expression_t *expr_back = expr->back;
            free(expr);
            expr = expr_back;
        }
        free(rule->name);
        rule_t *rule_next = rule->next;
        free(rule);
        rule = rule_next;
    }
}

int main() {
    int ret = EXIT_SUCCESS;
    lex_t lex;
    lex_init(&lex);
    char c = getchar();
    bool lex_ok = true;
    while (c != EOF && lex_ok) {
        lex_ok = lex_consume(&lex, c);
        c = getchar();
    };
    if (lex_ok) {
        if (0)
            lex_print(&lex);
        pars_t pars;
        pars_init(&pars);
        if (pars_parse(&pars, &lex) == PSOK) {
            pars_print(&pars);
        } else {
            pars_print_err(&pars);
            ret = EXIT_FAILURE;
        }
        pars_deinit(&pars);
    } else {
        lex_print_err(&lex);
        ret = EXIT_FAILURE;
    }
    lex_deinit(&lex);
    return ret;
}
