#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include <cinttypes>

// Implementing json-like grammar
// { key: 1765, key2: { keyy: 665, keyz: 0xfF }, keyt: 0b01, }
// No Arrays, just map. No quotes.

#define LINESIZE 1*1000

const char *printable_ascii[256] = {
    "<0x00>", "<0x01>", "<0x02>", "<0x03>", "<0x04>", "<0x05>",
    "<0x06>", "<0x07>", "<0x08>", "<0x09>", "<0x0A>", "<0x0B>",
    "<0x0C>", "<0x0D>", "<0x0E>", "<0x0F>", "<0x10>", "<0x11>",
    "<0x12>", "<0x13>", "<0x14>", "<0x15>", "<0x16>", "<0x17>",
    "<0x18>", "<0x19>", "<0x1A>", "<0x1B>", "<0x1C>", "<0x1D>",
    "<0x1E>", "<0x1F>", " ", "!", "\"", "#", "$", "%", "&", "'",
    "(", ")", "*", "+", ",", "-", ".", "/", "0", "1", "2", "3",
    "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
    "@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "[", "\\", "]", "^", "_", "`", "a", "b", "c",
    "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "{",
    "|", "}", "~", "<0x7F>", "<0x80>", "<0x81>", "<0x82>",
    "<0x83>", "<0x84>", "<0x85>", "<0x86>", "<0x87>", "<0x88>",
    "<0x89>", "<0x8A>", "<0x8B>", "<0x8C>", "<0x8D>", "<0x8E>",
    "<0x8F>", "<0x90>", "<0x91>", "<0x92>", "<0x93>", "<0x94>",
    "<0x95>", "<0x96>", "<0x97>", "<0x98>", "<0x99>", "<0x9A>",
    "<0x9B>", "<0x9C>", "<0x9D>", "<0x9E>", "<0x9F>", "<0xA0>",
    "<0xA1>", "<0xA2>", "<0xA3>", "<0xA4>", "<0xA5>", "<0xA6>",
    "<0xA7>", "<0xA8>", "<0xA9>", "<0xAA>", "<0xAB>", "<0xAC>",
    "<0xAD>", "<0xAE>", "<0xAF>", "<0xB0>", "<0xB1>", "<0xB2>",
    "<0xB3>", "<0xB4>", "<0xB5>", "<0xB6>", "<0xB7>", "<0xB8>",
    "<0xB9>", "<0xBA>", "<0xBB>", "<0xBC>", "<0xBD>", "<0xBE>",
    "<0xBF>", "<0xC0>", "<0xC1>", "<0xC2>", "<0xC3>", "<0xC4>",
    "<0xC5>", "<0xC6>", "<0xC7>", "<0xC8>", "<0xC9>", "<0xCA>",
    "<0xCB>", "<0xCC>", "<0xCD>", "<0xCE>", "<0xCF>", "<0xD0>",
    "<0xD1>", "<0xD2>", "<0xD3>", "<0xD4>", "<0xD5>", "<0xD6>",
    "<0xD7>", "<0xD8>", "<0xD9>", "<0xDA>", "<0xDB>", "<0xDC>",
    "<0xDD>", "<0xDE>", "<0xDF>", "<0xE0>", "<0xE1>", "<0xE2>",
    "<0xE3>", "<0xE4>", "<0xE5>", "<0xE6>", "<0xE7>", "<0xE8>",
    "<0xE9>", "<0xEA>", "<0xEB>", "<0xEC>", "<0xED>", "<0xEE>",
    "<0xEF>", "<0xF0>", "<0xF1>", "<0xF2>", "<0xF3>", "<0xF4>",
    "<0xF5>", "<0xF6>", "<0xF7>", "<0xF8>", "<0xF9>", "<0xFA>",
    "<0xFB>", "<0xFC>", "<0xFD>", "<0xFE>", "<0xFF>",
};
static const char *sym_to_printable(char sym)
{
    return printable_ascii[size_t(sym)];
}

static bool is_single(char sym)
{
    return sym == ' '
        || sym == '\t'
        || sym == '\n'
        || sym == '\r'
        || sym == '{'
        || sym == '}'
        || sym == ':'
        || sym == ','
        || sym == '-';
}

static bool is_num_begin(char sym)
{
    return sym >= '0' && sym <= '9';
}

static bool is_num_continue(char sym)
{
    return is_num_begin(sym);
}

static bool is_alphanum_begin(char sym)
{
    return (sym >= 'A' && sym <= 'Z')
        || (sym >= 'a' && sym <= 'z');
}

static bool is_alphanum_continue(char sym)
{
    return is_num_begin(sym) || is_alphanum_begin(sym);
}
struct Token
{
    enum class Variant
    {
        kUndef = 0,
        kSpace,
        kTab,
        kNewline,
        kCarret,
        kNum,
        kDash,
        kColon,
        kDelim,
        kOpen,
        kClose,
        kAlphanum,
    };
    Variant variant;
    std::string source; // string_view maybe?
    uint32_t linenum;
    uint32_t offset;
    uint32_t len;

    static Token FromSym(char sym, std::string line,
            uint32_t linenum, uint32_t offset)
    {
        auto variant = Variant::kUndef;
        switch (sym)
        {
        case ' ': variant = Variant::kSpace; break;
        case '\t': variant = Variant::kTab; break;
        case '\n': variant = Variant::kNewline; break;
        case '\r': variant = Variant::kCarret; break;
        case '-': variant = Variant::kDash; break;
        case ':': variant = Variant::kColon; break;
        case ',': variant = Variant::kDelim; break;
        case '{': variant = Variant::kOpen; break;
        case '}': variant = Variant::kClose; break;
        }
        return {
            variant,
            line,
            linenum,
            offset,
            1,
        };
    }
    static Token Alphanum(std::string line,
            uint32_t linenum, uint32_t offset, uint32_t len)
    {
        return {
            Variant::kAlphanum,
            line,
            linenum,
            offset,
            len,
        };
    }
    static Token Num(std::string line,
            uint32_t linenum, uint32_t offset, uint32_t len)
    {
        return {
            Variant::kNum,
            line,
            linenum,
            offset,
            len,
        };
    }
};

static const char *token_variant_to_printable(Token::Variant v)
{
    switch (v)
    {
    case Token::Variant::kUndef: return "Undef";
    case Token::Variant::kSpace: return "Space";
    case Token::Variant::kTab: return "Tab";
    case Token::Variant::kNewline: return "Newline";
    case Token::Variant::kCarret: return "Carret";
    case Token::Variant::kNum: return "Num";
    case Token::Variant::kDash: return "Dash";
    case Token::Variant::kColon: return "Colon";
    case Token::Variant::kDelim: return "Delim";
    case Token::Variant::kOpen: return "Open";
    case Token::Variant::kClose: return "Close";
    case Token::Variant::kAlphanum: return "Alphanum";
    }
}

using TokenizedLine = std::vector<const Token>;

static void print_tokens(TokenizedLine line)
{
    for (auto t: line)
    {
        printf("%" PRIu32 ":%" PRIu32 ":%" PRIu32 ":%s:`",
                t.linenum,
                t.offset,
                t.len,
                token_variant_to_printable(t.variant));
        std::string str(&t.source[t.offset], t.len);
        for (char sym: str)
        {
            printf("%s", sym_to_printable(sym));
        }
        printf("`\n");
    }
}

struct TokenError
{
    std::string expected;
    std::string line;
    uint32_t linenum;
    uint32_t offset;
};

class LineTokenizer
{
private:
    enum class State
    {
        kIdle,
        kAlphanum,
        kNum,
    };

public:
    LineTokenizer(uint32_t line_number, std::string line)
        : _line(line), _linenum(line_number)
    {
        for (char sym: line)
        {
            if (sym == 0) break;
            if (!consume(sym)) break;
        }
    }

    ~LineTokenizer(void) {}

    const std::vector<const Token>& Tokens(void) {
        return _tokens;
    }

    bool HasError(void) { return _have_error; }

    TokenError Error(void) const { return _error; }

    TokenizedLine Tokens(void) const { return _tokens; }

private:
    /** Consume codepoint */
    bool consume(char cp)
    {
        assert(!_have_error);
        switch (_state)
        {
        case State::kIdle:
            consume_idle(cp);
            break;
        case State::kAlphanum:
            consume_alphanum(cp);
            break;
        case State::kNum:
            consume_num(cp);
            break;
        }
        return !_have_error;
    }

    void consume_idle(char sym)
    {
        if (is_single(sym))
        {
            _tokens.push_back(Token::FromSym(
                        sym, _line, _linenum, _offset));
            _offset++;
        }
        else if (is_num_begin(sym))
        {
            _num += sym;
            _state = State::kNum;
        }
        else if (is_alphanum_begin(sym))
        {
            _alphanum += sym;
            _state = State::kAlphanum;
        }
        else
        {
            error();
        }
    }

    void consume_alphanum(char sym)
    {
        if (is_alphanum_continue(sym))
        {
            _alphanum += sym;
        }
        else if (is_num_begin(sym))
        {
            _tokens.push_back(Token::Alphanum(
                        _line,
                        _linenum,
                        _offset,
                        _alphanum.length()));
            _offset += _alphanum.length();
            _alphanum.clear();
            _num += sym;
            _state = State::kNum;
        }
        else if (is_single(sym))
        {
            _tokens.push_back(Token::Alphanum(
                        _line,
                        _linenum,
                        _offset,
                        _alphanum.length()));
            _offset += _alphanum.length();
            _alphanum.clear();
            _tokens.push_back(Token::FromSym(
                        sym, _line, _linenum, _offset));
            _offset++;
            _state = State::kIdle;
        }
        else
        {
            error();
        }
    }

    void consume_num(char sym)
    {
        if (is_num_continue(sym))
        {
            _num += sym;
        }
        else if (is_alphanum_begin(sym))
        {
            _tokens.push_back(Token::Num(
                        _line,
                        _linenum,
                        _offset,
                        _num.length()));
            _offset += _num.length();
            _num.clear();
            _alphanum += sym;
            _state = State::kAlphanum;
        }
        else if (is_single(sym))
        {
            _tokens.push_back(Token::Num(
                        _line,
                        _linenum,
                        _offset,
                        _num.length()));
            _offset += _num.length();
            _num.clear();
            _tokens.push_back(Token::FromSym(
                        sym, _line, _linenum, _offset));
            _offset++;
            _state = State::kIdle;
        }
        else
        {
            error();
        }
    }

    void error(void)
    {
        _error = TokenError{
            expected_text(_state),
            _line,
            _linenum,
            _offset,
        };
        _have_error = true;
    }

    static std::string expected_text(State state)
    {
        switch (state)
        {
        case State::kIdle:
            return "`{`, `}`, `-`, `,`, <lf>, <cr>, <tab>, "
                "<space>, [a-zA-Z] or [0-9]";
        case State::kAlphanum:
            return "[a-zA-Z0-9], `{`, `}`, `-`, `,`, "
                "<lf>, <cr>, <tab>or <space>";
        case State::kNum:
            return "[0-9], `{`, `}`, `-`, `,`, [a-zA-Z], "
                "<lf>, <cr>, <tab>, or <space>";
        }
        assert(false);
        return "<nil>";
    }

    State _state{State::kIdle};
    std::vector<const Token> _tokens{};
    std::string _alphanum{};
    std::string _num{};
    std::string _line{};
    uint32_t _linenum{};
    uint32_t _offset{};
    struct Token _current{};
    TokenError _error{};
    bool _have_error{};
};

struct Value
{
    enum class Variant
    {
        kNumber;
        kMap;
    };
};

class Parser
{
};

int main()
{
    std::string str(LINESIZE, '\0');
    uint32_t linenum(1);
    while (fgets(&str[0], LINESIZE, stdin) != NULL)
    {
        LineTokenizer tokenizer{linenum, str};
        if (tokenizer.HasError())
        {
            TokenError e = tokenizer.Error();
            printf(
                    "<stdin>:%" PRIu32 ":%" PRIu32 ": "
                    "unexpected `%s`, expected: %s",
                    e.linenum,
                    e.offset,
                    sym_to_printable(e.line[e.offset]),
                    e.expected.c_str());
            return 1;
        }
        else
        {
            printf("We have %zu tokens\n",
                    tokenizer.Tokens().size());
            print_tokens(tokenizer.Tokens());
        }
        linenum++;
    }
}
