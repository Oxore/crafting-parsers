import sys

class Tokenizer:
    CTX_NO = 'NO'
    CTX_NUMBER = 'NUMBER'
    SINGLE_SYMBOLS = ['[', ',', ']']
    SPACE_SYMBOLS = [' ', '\t', '\r', '\n']
    DEC_NUMBER_SYMBOLS = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9']

    def __init__(self):
        self.tokens = []

    def consume(self, line: str):
        self.__context = self.CTX_NO
        number = ''
        for c in line:
            if self.__context is self.CTX_NUMBER:
                if c in self.DEC_NUMBER_SYMBOLS:
                    number += c
                    continue
                else:
                    self.tokens.append(number)
                    self.__context = self.CTX_NO
                    number = ''

            if c in self.SINGLE_SYMBOLS:
                self.tokens.append(c)
            elif c in self.SPACE_SYMBOLS:
                continue
            elif c in self.DEC_NUMBER_SYMBOLS:
                number += c
                self.__context = self.CTX_NUMBER
            else:
                print('Unexpected symbol; \'{}\''.format(c))
                sys.exit(1)


class Parser:
    EXPECT_NUMBER_OR_LIST = 'number or \'[\''
    EXPECT_NUMBER_OR_LIST_OR_CLOSE = 'number or \'[\' or \']\''
    EXPECT_DIVIDER_OR_CLOSE = '\',\' or \']\''
    EXPECT_DIVIDER = '\',\''
    __level = 0
    __stack = []
    __current_list = []
    __context = EXPECT_NUMBER_OR_LIST

    def __init__(self):
        pass

    def __is_number(self, t: str):
        c = ord(t[0])
        return c >= 0x30 and c <= 0x39

    def __is_open_bracket(self, t: str):
        return t == '['

    def __is_close_bracket(self, t: str):
        return t == ']'

    def __is_divider(self, t: str):
        return t == ','

    def __accept_number(self, t: str):
        self.__current_list.append(int(t))
        if self.__level == 0:
            self.__context = self.EXPECT_DIVIDER
        else:
            self.__context = self.EXPECT_DIVIDER_OR_CLOSE

    def __accept_open_bracket(self, t: str):
        self.__level += 1
        self.__stack.insert(0, self.__current_list)
        self.__current_list = []
        self.__context = self.EXPECT_NUMBER_OR_LIST_OR_CLOSE

    def __accept_close_bracket(self, t: str):
        parent_list = self.__stack.pop(0)
        parent_list.append(self.__current_list)
        self.__current_list = parent_list
        self.__level -= 1
        if self.__level == 0:
            self.__context = self.EXPECT_DIVIDER
        else:
            self.__context = self.EXPECT_DIVIDER_OR_CLOSE

    def __accept_divider(self, t: str):
        if self.__level == 0:
            self.__context = self.EXPECT_NUMBER_OR_LIST
        else:
            self.__context = self.EXPECT_NUMBER_OR_LIST_OR_CLOSE

    def __unexpected(self, t: str, expected: str):
        print('Unexpected token; \'{}\'. Expected {}.'.format(t, expected))
        sys.exit(1)

    def parse(self, tokens: list):
        for t in tokens:
            if self.__context == self.EXPECT_DIVIDER:
                if self.__is_divider(t):
                    self.__accept_divider(t)
                else:
                    self.__unexpected(t, self.__context)
            elif self.__context == self.EXPECT_DIVIDER_OR_CLOSE:
                if self.__is_divider(t):
                    self.__accept_divider(t)
                elif self.__is_close_bracket(t):
                    self.__accept_close_bracket(t)
                else:
                    self.__unexpected(t, self.__context)
            elif self.__context == self.EXPECT_NUMBER_OR_LIST:
                if self.__is_number(t):
                    self.__accept_number(t)
                elif self.__is_open_bracket(t):
                    self.__accept_open_bracket(t)
                else:
                    self.__unexpected(t, self.__context)
            elif self.__context == self.EXPECT_NUMBER_OR_LIST_OR_CLOSE:
                if self.__is_number(t):
                    self.__accept_number(t)
                elif self.__is_open_bracket(t):
                    self.__accept_open_bracket(t)
                elif self.__is_close_bracket(t):
                    self.__accept_close_bracket(t)
                else:
                    self.__unexpected(t, self.__context)
            elif self.__level != 0:
                print('Invalid self.__context ' + self.__context)
                sys.exit(1)

        if self.__stack:
            print('Unexpected end of tokens. Expected ' + self.__context + '.')
            sys.exit(1)

        return self.__current_list

if __name__ == "__main__":
    t = Tokenizer()
    for line in sys.stdin:
        t.consume(line)
    p = Parser()
    ast = p.parse(t.tokens)
    print(ast)
