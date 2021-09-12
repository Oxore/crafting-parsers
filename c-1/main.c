// TODO use stack when printing nested objects
// TODO use stack when destroying nested objects
// TODO walk through stack if it is not null in pars_destroy

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define BUFSIZE 1024

typedef enum {
    SIDLE = 0,
    SNUM,
    SSTRING,
} lex_state_t;

typedef enum {
    TSTRING = 0,
    TNUM = 1,
    TLCURLY = 2,
    TLBRACKET = 3,
    TRCURLY,
    TRBRACKET,
    TCOMMA,
    TCOLON,
} toktype_t;

typedef struct token_s
{
    struct token_s *next;
    toktype_t type;
    const char *source;
    unsigned long line, col;
    size_t offset, len;
} token_t;

typedef struct
{
    lex_state_t state;
    char *source;
    token_t *first;
    token_t *last;
    unsigned long line, col;
    size_t offset;
    bool error;
} lex_t;

typedef enum
{
    EMAP = 0,
    EARR,
    EKV,
    ESTRING,
    ENUM,
} elemtype_t;

struct elem_s;
struct elem_literal_s;

typedef struct elem_kv_s
{
    char *key;
    struct elem_s *value;
} elem_kv_t;

typedef struct
{
    struct elem_s *first;
    struct elem_s *last;
} elem_arr_t;

union elem_u
{
    elem_arr_t map;
    elem_arr_t arr;
    elem_kv_t kv;
    char *literal;
};

typedef struct elem_s
{
    struct elem_s *next;
    elemtype_t type;
    union elem_u data;
} elem_t;

typedef enum
{
    PSKEY = 0,
    PSKVDIV,
    PSVAL,
    PSMAPDIV,
    PSARRELEM,
    PSARRDIV,
} pstate_t;

typedef struct stack_item_s
{
    struct stack_item_s *prev;
    elem_t *elem;
    pstate_t state;
} stack_item_t;

typedef struct
{
    elem_t *root_map;
    stack_item_t *stack;
    bool error;
    unsigned long line, col;
} parsing_t;

static bool is_numeric(char c)
{
    return (c >= '0' && c <= '9');
}

static bool is_alphabetic(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c == '_');
}

static bool is_alphanumeric(char c)
{
    return is_numeric(c) || is_alphabetic(c);
}

static void newline(lex_t *lex)
{
    lex->state = SIDLE;
    lex->line++;
    lex->col = 0;
}

static void space(lex_t *lex)
{
    lex->state = SIDLE;
}

static token_t *add_token(lex_t *lex, toktype_t type)
{
    token_t *token = calloc(1, sizeof(token_t));
    token->type = type;
    token->line = lex->line;
    token->col = lex->col;
    token->offset = lex->offset;
    token->len = 1;
    token->source = lex->source;

    lex->state = SIDLE;
    if (lex->last) {
        lex->last->next = token;
        lex->last = lex->last->next;
    }
    else
    {
        lex->first = token;
        lex->last = token;
    }
    return token;
}

static void number_begin(lex_t *lex)
{
    add_token(lex, TNUM);
    lex->state = SNUM;
}

static void number_continue(lex_t *lex)
{
    lex->last->len++;
}

static void string_continue(lex_t *lex)
{
    lex->last->len++;
}

static void string_begin(lex_t *lex)
{
    add_token(lex, TSTRING);
    lex->state = SSTRING;
}

static void lex_error(lex_t *lex)
{
    lex->error = true;
}

void lex_init(lex_t *lex)
{
    *lex = (lex_t){.line=1};
    lex->source = calloc(1, BUFSIZE);
}

void lex_destroy(lex_t *lex)
{
    token_t *token = lex->first;
    while (token)
    {
        token_t *slated = token;
        token = slated->next;
        free(slated);
    }
    free(lex->source);
}

bool lex_consume(lex_t *ctx, char c)
{
    if (c == '[')
        add_token(ctx, TLBRACKET);
    else if (c == ']')
        add_token(ctx, TRBRACKET);
    else if (c == '{')
        add_token(ctx, TLCURLY);
    else if (c == '}')
        add_token(ctx, TRCURLY);
    else if (c == ':')
        add_token(ctx, TCOLON);
    else if (c == ',')
        add_token(ctx, TCOMMA);
    else if (c == ' ' || c == '\t')
        space(ctx);
    else if (c == '\n')
    {
        newline(ctx);
        ctx->source[ctx->offset] = c;
        ctx->offset++;
        return !ctx->error;
    }
    else
    {
        switch (ctx->state)
        {
        case SIDLE:
            if (is_numeric(c))
                number_begin(ctx);
            else if (is_alphabetic(c))
                string_begin(ctx);
            else
                lex_error(ctx);
            break;
        case SNUM:
            if (is_numeric(c))
                number_continue(ctx);
            else
                lex_error(ctx);
            break;
        case SSTRING:
            if (is_alphanumeric(c))
                string_continue(ctx);
            else
                lex_error(ctx);
            break;
        default:
            lex_error(ctx);
            break;;
        }
    }

    ctx->source[ctx->offset] = c;
    ctx->offset++;
    ctx->col++;

    return !ctx->error;
}

void lex_print(lex_t *ctx)
{
    token_t *token = ctx->first;
    printf("[");
    while (token)
    {
        const char *separator = ((token)->next ? ", ": "");
        const char *fragment = ctx->source + (token)->offset;
        printf("\"%.*s\"%s", (int)(token)->len, fragment, separator);
        token = (token)->next;
    }
    printf("]\n");
}

static void pars_error(parsing_t *p)
{
    p->error = true;
}

void pars_init(parsing_t *p)
{
    *p = (parsing_t){0};
    p->root_map = calloc(1, sizeof(*p->root_map));
    p->stack = calloc(1, sizeof(*p->stack));
    p->stack->elem = p->root_map;
}

static void elem_print(elem_t *e, size_t level, bool value)
{
    if (e->type == EKV)
    {
        for (size_t i = 0; i < level; i++)
            printf("  ");
        printf("%s: ", e->data.kv.key);
        elem_print(e->data.kv.value, level, true);
    }
    else if (e->type == EMAP)
    {
        if (!value)
            for (size_t i = 0; i < level; i++)
                printf("  ");
        if (e->data.arr.first)
        {
            printf("{\n");
            elem_t *kv = e->data.map.first;
            while (kv)
            {
                elem_print(kv, level + 1, false);
                kv = kv->next;
            }
            for (size_t i = 0; i < level; i++)
                printf("  ");
            printf("},\n");
        }
        else
            printf("{},\n");
    }
    else if (e->type == EARR)
    {
        if (!value)
            for (size_t i = 0; i < level; i++)
                printf("  ");
        if (e->data.arr.first)
        {
            printf("[\n");
            elem_t *arr_item = e->data.arr.first;
            while (arr_item)
            {
                elem_print(arr_item, level + 1, false);
                arr_item = arr_item->next;
            }
            for (size_t i = 0; i < level; i++)
                printf("  ");
            printf("],\n");
        }
        else
            printf("[],\n");
    }
    else // literal
    {
        if (!value)
            for (size_t i = 0; i < level; i++)
                printf("  ");
        printf("%s,\n", e->data.literal);
    }
}

void pars_print(parsing_t *p)
{
    elem_t *e = p->stack->elem->data.map.first;
    while (e)
    {
        elem_print(e, 0, false);
        e = e->next;
    }
}

static void kvkey(parsing_t *p, const token_t *t)
{
    elem_t *kv = calloc(1, sizeof(elem_t));
    kv->type = EKV;
    char *key = kv->data.kv.key = calloc(1, t->len + 1);
    memcpy(key, t->source + t->offset, t->len);

    if (p->stack->elem->data.map.last)
        p->stack->elem->data.map.last->next = kv;
    else
        p->stack->elem->data.map.first = kv;
    p->stack->elem->data.map.last = kv;

    p->stack->state = PSKVDIV;
}

static void kvval(parsing_t *p, const token_t *t)
{
    elem_t *object =  calloc(1, sizeof(elem_t));
    if (t->type == TNUM)
        object->type = ENUM;
    else
        object->type = ESTRING;
    char *value = object->data.literal = calloc(1, t->len + 1);
    memcpy(value, t->source + t->offset, t->len);

    p->stack->elem->data.map.last->data.kv.value = object;

    p->stack->state = PSMAPDIV;
}

static void arrelem(parsing_t *p, const token_t *t)
{
    elem_t *object = calloc(1, sizeof(elem_t));
    if (t->type == TNUM)
        object->type = ENUM;
    else
        object->type = ESTRING;
    char *value = object->data.literal = calloc(1, t->len + 1);
    memcpy(value, t->source + t->offset, t->len);

    if (p->stack->elem->data.arr.last)
        p->stack->elem->data.arr.last->next = object;
    else
        p->stack->elem->data.arr.first = object;
    p->stack->elem->data.arr.last = object;

    p->stack->state = PSARRDIV;
}

static void push(parsing_t *p, const token_t *t)
{
    stack_item_t *child = calloc(1, sizeof(stack_item_t));
    child->prev = p->stack;
    p->stack = child;
    elem_t *elem = calloc(1, sizeof(elem_t));
    child->elem = elem;

    if (t->type == TLBRACKET)
    {
        p->stack->state = PSARRELEM;
        elem->type = EARR;
    }
    else // TLCURLY
    {
        p->stack->state = PSKEY;
        elem->type = EMAP;
    }
}

static void pop(parsing_t *p)
{
    stack_item_t *parent = p->stack->prev;

    if (parent->state == PSVAL)
    {
        parent->state = PSMAPDIV;
        parent->elem->data.map.last->data.kv.value = p->stack->elem;
    }
    else // PSARRELEM
    {
        parent->state = PSARRDIV;
        if (parent->elem->data.arr.last)
            parent->elem->data.arr.last->next = p->stack->elem;
        else
            parent->elem->data.arr.first = p->stack->elem;
        parent->elem->data.arr.last = p->stack->elem;
    }

    free(p->stack);
    p->stack = parent;
}

static bool token_type_mismatch(toktype_t t, elemtype_t e)
{
    return !(t > 3 ||
            (t == TSTRING && e == ESTRING) ||
            (t == TNUM && e == ENUM) ||
            (t == TLCURLY && e == EMAP) ||
            (t == TLBRACKET && e == EARR));
}

static bool pars_consume(parsing_t *p, const token_t *t)
{
    if (p->error)
        return false;

    p->line = t->line;
    p->col = t->col;

    switch (p->stack->state)
    {
    case PSKEY:
        if (t->type == TSTRING)
            kvkey(p, t);
        else if (t->type == TRCURLY)
            pop(p);
        else
            pars_error(p);
        break;
    case PSKVDIV:
        if (t->type == TCOLON)
            p->stack->state = PSVAL;
        else
            pars_error(p);
        break;
    case PSVAL:
        if (t->type == TSTRING || t->type == TNUM)
            kvval(p, t);
        else if (t->type == TLCURLY || t->type == TLBRACKET)
            push(p, t);
        else
            pars_error(p);
        break;
    case PSMAPDIV:
        if (t->type == TCOMMA)
            p->stack->state = PSKEY;
        else if (t->type == TRCURLY)
            pop(p);
        else
            pars_error(p);
        break;
    case PSARRELEM:
        {
            elem_t *first = p->stack->elem->data.arr.first;
            if (first && token_type_mismatch(t->type, first->type))
            {
                // Heterogeneous arrays are forbidden explicitly
                pars_error(p);
            }
            if (t->type == TSTRING || t->type == TNUM)
                arrelem(p, t);
            else if (t->type == TLCURLY || t->type == TLBRACKET)
                push(p, t);
            else if (t->type == TRBRACKET)
                pop(p);
            else
                pars_error(p);
        }
        break;
    case PSARRDIV:
        if (t->type == TCOMMA)
            p->stack->state = PSARRELEM;
        else if (t->type == TRBRACKET)
            pop(p);
        else
            pars_error(p);
        break;
    }

    return !p->error;
}

static void elem_destroy(elem_t *e)
{
    if (e->type == EKV)
    {
        free(e->data.kv.key);
        if (e->data.kv.value)
            // TODO use p->stack instead of recursion
            elem_destroy(e->data.kv.value);
    }
    else if (e->type == EMAP)
    {
        elem_t *kv = e->data.map.first;
        while (kv)
        {
            elem_t *slated = kv;
            kv = slated->next;
            elem_destroy(slated);
        }
    }
    else if (e->type == EARR)
    {
        elem_t *arr_item = e->data.arr.first;
        while (arr_item)
        {
            elem_t *slated = arr_item;
            arr_item = slated->next;
            elem_destroy(slated);
        }
    }
    else // literal
        free(e->data.literal);
    free(e);
}

void pars_destroy(parsing_t *p)
{
    while (p->stack->prev)
        pop(p);
    elem_t *e = p->stack->elem->data.map.first;
    while (e)
    {
        elem_t *slated = e;
        e = slated->next;
        elem_destroy(slated);
    }
    free(p->stack);
    free(p->root_map);
}

bool pars_parse(parsing_t *p, const lex_t *lex)
{
    for (token_t* t = lex->first; t; t = t->next)
        if (!pars_consume(p, t))
            return false;
    return true;
}

bool pars_finish(parsing_t *p)
{
    pstate_t state = p->stack->state;
    if (p->stack->prev || (state != PSKEY && state != PSMAPDIV))
        p->error = true;

    return !p->error;
}

int main()
{
    lex_t lex;
    lex_init(&lex);
    while (true)
    {
        char c = getchar();
        if (c == EOF)
            break;

        if (!lex_consume(&lex, c))
        {
            printf("lexing error <stdin>:%lu:%lu\n", lex.line, lex.col);
            break;
        }
    }
    if (!lex.error)
    {
        //lex_print(&lex);
        parsing_t p;
        pars_init(&p);
        bool parsing_ok = pars_parse(&p, &lex);
        bool finalizing_ok = pars_finish(&p);
        if (!parsing_ok)
            printf(
                    "parsing error <stdin>:%lu:%lu: "
                    "Invalid token in current state\n",
                    p.line,
                    p.col);
        else if (!finalizing_ok)
            printf(
                    "parsing error <stdin>:%lu:%lu: incomplete input\n",
                    p.line,
                    p.col);
        else
            pars_print(&p);
        pars_destroy(&p);
    }
    lex_destroy(&lex);
}
