// TODO Thoroughly test
// XXX  Forbid heterogeneous arrays? It seems difficult for error reporting implementation.

const std = @import("std");
const log = std.log;
const mem = std.mem;
const assert = std.debug.assert;

const TokenType = enum {
    const Self = @This();

    lbrace,
    rbrace,
    lbracket,
    rbracket,
    colon,
    comma,
    number,
    word,

    pub fn format(
        self: Self,
        comptime fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = options;
        if (fmt.len == 1 and fmt[0] == 's') {
            switch (self) {
                .lbrace => _ = try writer.write("lbrace"),
                .rbrace => _ = try writer.write("rbrace"),
                .lbracket => _ = try writer.write("lbracket"),
                .rbracket => _ = try writer.write("rbracket"),
                .colon => _ = try writer.write("colon"),
                .comma => _ = try writer.write("comma"),
                .number => _ = try writer.write("number"),
                .word => _ = try writer.write("word"),
            }
        } else @compileError(
            "Only {s} specifier can be used for " ++ @typeName(Self) ++ " formatting",
        );
    }
};

const Token = struct {
    next: ?*Token = null,
    type: TokenType = TokenType.word,
    offset: usize = 0,
    line: usize = 0,
    col: usize = 0,
    len: usize = 0,
};

const TokenFormatter = struct {
    const Self = @This();

    token: *const Token,
    source: []const u8,

    fn fromToken(token: *const Token, source: []const u8) Self {
        return Self{
            .token = token,
            .source = source,
        };
    }

    pub fn format(
        self: Self,
        comptime fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = options;
        if (fmt.len == 1 and fmt[0] == 's') {
            try writer.print("{s}", .{
                self.source[self.token.offset .. self.token.offset + self.token.len],
            });
        } else if (fmt.len == 1 and fmt[0] == 'd') {
            try writer.print("{s}\"{s}\"", .{
                self.token.type,
                self.source[self.token.offset .. self.token.offset + self.token.len],
            });
        } else @compileError(
            "Only {s} and {d} specifiers can be used for " ++ @typeName(Self) ++ " formatting",
        );
    }
};

const Lex = struct {
    const LexState = enum {
        idle,
        word,
        number,
        pub fn expected(self: @This()) []const u8 {
            switch (self) {
                .idle => return "\"[a-zA-Z_]\", \"[0-9]\", " ++
                    "\"{\", \"}\", \"[\", \"]\", \":\" or \",\"",
                .word => return "\"[a-zA-Z0-9_]\" to proceed the word or else " ++
                    "\"{\", \"}\", \"[\", \"]\", \":\" or \",\"",
                .number => return "\"[0-9]\"",
            }
        }
    };

    const Self = @This();

    state: LexState = LexState.idle,
    offset: usize = 0,
    line: usize = 0,
    col: usize = 0,
    have_err: bool = false,
    first: ?*Token = null,
    last: ?*Token = null,
    allocator: mem.Allocator,
    source: []u8,

    pub fn init(allocator: mem.Allocator) Self {
        return Self{
            .allocator = allocator,
            .source = allocator.alloc(u8, 1024) catch unreachable,
        };
    }

    fn addToken(self: *Self, t: TokenType) void {
        if (t == TokenType.number) {
            self.state = LexState.number;
        } else if (t == TokenType.word) {
            self.state = LexState.word;
        } else self.state = LexState.idle;
        const token: *Token = self.allocator.create(Token) catch unreachable;
        token.* = Token{
            .next = null,
            .type = t,
            .offset = self.offset,
            .line = self.line,
            .col = self.col,
            .len = 1,
        };
        if (self.last) |last| {
            last.next = token;
        } else {
            self.first = token;
        }
        self.last = token;
    }

    fn continueToken(self: *Self, token: *Token) void {
        _ = self;
        token.len += 1;
    }

    fn simpleToktype(byte: u8) TokenType {
        return switch (byte) {
            '{' => TokenType.lbrace,
            '}' => TokenType.rbrace,
            '[' => TokenType.lbracket,
            ']' => TokenType.rbracket,
            ':' => TokenType.colon,
            ',' => TokenType.comma,
            else => unreachable,
        };
    }

    fn err(self: *Self, byte: u8) bool {
        self.source[self.offset] = byte;
        self.have_err = true;
        return !self.have_err;
    }

    fn consumeByte(self: *Self, byte: u8) bool {
        if (self.have_err)
            return false;
        switch (byte) {
            '{', '}', '[', ']', ':', ',' => |t| {
                self.addToken(simpleToktype(t));
            },
            ' ', '\t', '\n', '\r' => {
                self.state = LexState.idle;
            },
            '0'...'9' => |_| {
                if (self.state == LexState.number or self.state == LexState.word) {
                    self.continueToken(self.last.?);
                } else self.addToken(TokenType.number);
            },
            'A'...'Z',
            'a'...'z',
            '_',
            => |alpha| {
                if (self.state == LexState.number) {
                    return self.err(alpha);
                } else if (self.state == LexState.word) {
                    self.continueToken(self.last.?);
                } else self.addToken(TokenType.word);
            },
            else => |char| return self.err(char),
        }
        self.source[self.offset] = byte;
        self.offset += 1;
        if (byte == '\n') {
            self.col = 0;
            self.line += 1;
        } else {
            self.col += 1;
        }
        return !self.have_err;
    }

    pub fn consume(self: *Self, data: []const u8) bool {
        for (data) |byte|
            if (!self.consumeByte(byte))
                return false;
        return true;
    }

    pub fn deinit(self: *Self) void {
        var token = self.first;
        while (token) |t| {
            token = t.next;
            self.allocator.destroy(t);
        }
        self.first = null;
        self.last = null;
        self.allocator.free(self.source);
    }

    fn delimiter(present: bool) []const u8 {
        if (present) return ", ";
        return "";
    }

    pub fn print(self: *Self, writer: *std.fs.File.Writer) void {
        var token = self.first;
        _ = writer.write("[") catch unreachable;
        while (token) |t| {
            _ = writer.print("{d}{s}", .{
                TokenFormatter.fromToken(t, self.source),
                delimiter(t.next != null),
            }) catch unreachable;
            token = t.next;
        }
        _ = writer.write("]\n") catch unreachable;
    }

    pub fn errorFormatted(self: *Self) LexErrFormatter {
        assert(self.have_err);
        return LexErrFormatter{
            .line = self.line,
            .col = self.col,
            .char = self.source[self.offset],
            .expected = self.state.expected(),
        };
    }
};

const LexErrFormatter = struct {
    line: usize,
    col: usize,
    char: u8,
    expected: []const u8,

    pub fn format(
        self: @This(),
        comptime fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = options;
        if (fmt.len == 1 and fmt[0] == 's') {
            try writer.print("<stdin>:{d}:{d}: unexpected char \"{c}\", expected {s}", .{
                self.line,
                self.col,
                self.char,
                self.expected,
            });
        } else @compileError(
            "Only {s} specifier can be used for " ++ @typeName(@This()) ++ " formatting",
        );
    }
};

const NodeType = enum {
    kv,
    map,
    array,
    number,
    word,

    pub fn format(
        self: @This(),
        comptime fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = options;
        if (fmt.len == 1 and fmt[0] == 's') {
            switch (self) {
                .kv => _ = try writer.write("kv"),
                .map => _ = try writer.write("map"),
                .array => _ = try writer.write("array"),
                .number => _ = try writer.write("number"),
                .word => _ = try writer.write("word"),
            }
        } else @compileError(
            "Only {s} specifier can be used for " ++ @typeName(@This()) ++ " formatting",
        );
    }
};

const ListNode = struct {
    first: ?*Node = null,
    last: ?*Node = null,
};

const KVNode = struct {
    value: *Node,
    key: *Node,
};

const NodeUnion = union(NodeType) {
    map: ListNode,
    array: ListNode,
    kv: KVNode,
    number,
    word,
};

const Node = struct {
    const Self = @This();

    next: ?*Node = null,
    data: NodeUnion,
    offset: usize,
    line: usize,
    col: usize,
    len: usize,

    fn newRoot(allocator: mem.Allocator) *Self {
        var self = allocator.create(Node) catch unreachable;
        self.* = Self{
            .data = NodeUnion{ .map = ListNode{} },
            .offset = 0,
            .line = 0,
            .col = 0,
            .len = 0,
        };
        return self;
    }

    fn newArray(allocator: mem.Allocator, token: *Token) *Self {
        var self = allocator.create(Self) catch unreachable;
        self.* = Self{
            .data = NodeUnion{ .array = ListNode{} },
            .offset = token.offset,
            .line = token.line,
            .col = token.col,
            .len = token.len,
        };
        return self;
    }

    fn newMap(allocator: mem.Allocator, token: *Token) *Self {
        var self = allocator.create(Self) catch unreachable;
        self.* = Self{
            .data = NodeUnion{ .map = ListNode{} },
            .offset = token.offset,
            .line = token.line,
            .col = token.col,
            .len = token.len,
        };
        return self;
    }

    fn newWord(allocator: mem.Allocator, token: *Token) *Self {
        var self = allocator.create(Self) catch unreachable;
        self.* = Self{
            .data = NodeType.word,
            .offset = token.offset,
            .line = token.line,
            .col = token.col,
            .len = token.len,
        };
        return self;
    }

    fn newNumber(allocator: mem.Allocator, token: *Token) *Self {
        var self = allocator.create(Self) catch unreachable;
        self.* = Self{
            .data = NodeType.number,
            .offset = token.offset,
            .line = token.line,
            .col = token.col,
            .len = token.len,
        };
        return self;
    }

    fn newKeyValue(allocator: mem.Allocator, key: *Self, value: *Self) *Self {
        var self = allocator.create(Self) catch unreachable;
        self.* = Self{
            .data = NodeUnion{ .kv = KVNode{ .key = key, .value = value } },
            .offset = key.offset,
            .line = key.line,
            .col = key.col,
            .len = (value.offset + value.len) - key.offset,
        };
        return self;
    }

    fn destroy(self: *Self, allocator: mem.Allocator) void {
        switch (self.data) {
            .kv => |kv| {
                kv.value.destroy(allocator);
                kv.key.destroy(allocator);
                allocator.destroy(self);
            },
            .map, .array => |list| {
                var node_ptr = list.first;
                while (node_ptr) |node| {
                    var next = node.next;
                    node.destroy(allocator);
                    node_ptr = next;
                }
                allocator.destroy(self);
            },
            .number, .word => |_| allocator.destroy(self),
        }
    }
};

fn indent(writer: anytype, level_: usize) !void {
    var level = level_;
    while (level > 0) {
        _ = try writer.write("    ");
        level -= 1;
    }
}

const NodeFormatter = struct {
    const Self = @This();

    node: *const Node,
    source: []const u8,
    level: usize = 0,
    pretty: bool = false,
    in_pair: bool = false,

    fn default(node: *const Node, source: []const u8, level: usize) Self {
        return Self{
            .node = node,
            .source = source,
            .level = level,
        };
    }

    pub fn pretty(node: *const Node, source: []const u8, level: usize) Self {
        _ = level;
        return Self{
            .node = node,
            .source = source,
            .pretty = true,
        };
    }

    pub fn format(
        self: Self,
        fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = options;
        if (fmt.len == 1 and (fmt[0] == 's' or fmt[0] == 'd')) {
            switch (self.node.data) {
                .map => |map| {
                    if (self.pretty and !self.in_pair) try indent(writer, self.level);
                    _ = try writer.write("{");
                    var kv_ptr = map.first;
                    if (kv_ptr != null) {
                        if (self.pretty) _ = try writer.write("\n");
                        while (kv_ptr) |kv| {
                            var kv_fmt = Self.default(kv, self.source, self.level + 1);
                            kv_fmt.pretty = self.pretty;
                            try writer.print("{s}", .{kv_fmt});
                            if (kv.next != null) _ = try writer.write(",");
                            if (self.pretty) _ = try writer.write("\n");
                            kv_ptr = kv.next;
                        }
                        if (self.pretty) try indent(writer, self.level);
                    }
                    _ = try writer.write("}");
                },
                .kv => |kv| {
                    var key_fmt = Self.default(kv.key, self.source, self.level);
                    key_fmt.pretty = self.pretty;
                    var value_fmt = Self.default(kv.value, self.source, self.level);
                    value_fmt.in_pair = true;
                    value_fmt.pretty = self.pretty;
                    _ = try writer.print("{s}:{s}", .{ key_fmt, value_fmt });
                },
                .number, .word => |_| {
                    if (self.pretty and !self.in_pair) try indent(writer, self.level);
                    const start = self.node.offset;
                    const end = self.node.offset + self.node.len;
                    if (fmt[0] == 's') {
                        try writer.print("{s}", .{self.source[start..end]});
                    } else if (fmt[0] == 'd') {
                        _ = try writer.print("{s}\"{s}\"", .{
                            @as(NodeType, self.node.data),
                            self.source[start..end],
                        });
                    }
                },
                .array => |array| {
                    if (self.pretty and !self.in_pair) try indent(writer, self.level);
                    _ = try writer.write("[");
                    var arr_node_ptr = array.first;
                    if (arr_node_ptr != null) {
                        if (self.pretty) _ = try writer.write("\n");
                        while (arr_node_ptr) |arr_node| {
                            var arr_node_fmt = Self.default(arr_node, self.source, self.level + 1);
                            arr_node_fmt.pretty = self.pretty;
                            _ = try writer.print("{s}", .{arr_node_fmt});
                            if (arr_node.next != null) _ = try writer.write(",");
                            if (self.pretty) _ = try writer.write("\n");
                            arr_node_ptr = arr_node.next;
                        }
                        if (self.pretty) try indent(writer, self.level);
                    }
                    _ = try writer.write("]");
                },
            }
        }
    }
};

const Pars = struct {
    const ParsState = enum {
        key,
        key_root,
        kv_delim,
        value,
        map_delim,
        map_delim_root,
        arr_node,
        arr_delim,

        pub fn expected(self: @This()) []const u8 {
            switch (self) {
                .key => return "word\"[a-zA-Z][a-zA-Z0-9_]*\" or \"}\"",
                .key_root => return "word\"[a-zA-Z][a-zA-Z0-9_]*\"",
                .kv_delim => return "\":\"",
                .value => return "word\"[a-zA-Z][a-zA-Z0-9_]*\" or number\"[0-9]+\"",
                .map_delim => return "\",\" or \"}\"",
                .map_delim_root => return "\",\"",
                .arr_node => return "word\"[a-zA-Z][a-zA-Z0-9_]*\", number\"[0-9]+\", " ++
                    "\"{\", \"[\" or \"]\"",
                .arr_delim => return "\",\" or \"]\"",
            }
        }
    };

    const ParsErr = error{
        InvalidToken,
        EndOfStream,
    };

    const Self = @This();

    state: ParsState = ParsState.key_root,
    last_token: ?*Token = null,
    root: ?*Node = null,
    allocator: mem.Allocator,
    source: []const u8,

    pub fn init(allocator: mem.Allocator, source: []const u8) Self {
        return Self{
            .allocator = allocator,
            .source = source,
        };
    }

    fn err(self: *Self, token: *Token) ParsErr!void {
        self.last_token = token;
        return error.InvalidToken;
    }

    pub fn parse(self: *Self, tokens_: ?*Token) ParsErr!void {
        var tokens: ?*Token = tokens_;
        var token_ptr: *?*Token = &tokens;
        self.root = try self.parseMap(token_ptr);
    }

    fn parseKv(self: *Self, token_ptr: *?*Token) ParsErr!*Node {
        const key_token = token_ptr.* orelse return error.EndOfStream;
        self.last_token = token_ptr.*;
        if (key_token.type != TokenType.word) {
            try self.err(key_token);
        }
        const key = Node.newWord(self.allocator, key_token);
        errdefer key.destroy(self.allocator);
        token_ptr.* = key_token.next;
        self.state = ParsState.kv_delim;
        // Delimiter
        const colon_token = token_ptr.* orelse return error.EndOfStream;
        self.last_token = token_ptr.*;
        if (colon_token.type != TokenType.colon) {
            try self.err(colon_token);
        }
        token_ptr.* = colon_token.next;
        // The value
        self.state = ParsState.value;
        const value = try self.parseValue(token_ptr);
        const kv = Node.newKeyValue(self.allocator, key, value);
        return kv;
    }

    fn parseValue(self: *Self, token_ptr: *?*Token) ParsErr!*Node {
        if (self.state != ParsState.value and self.state != ParsState.arr_node)
            unreachable;
        const token = token_ptr.* orelse return error.EndOfStream;
        self.last_token = token_ptr.*;
        switch (token.type) {
            .rbrace,
            .rbracket,
            .colon,
            .comma,
            => unreachable,
            .lbrace => return self.parseMap(token_ptr),
            .lbracket => return self.parseArray(token_ptr),
            .number => return Node.newNumber(self.allocator, token),
            .word => return Node.newWord(self.allocator, token),
        }
    }

    pub fn parseMap(self: *Self, token_ptr: *?*Token) ParsErr!*Node {
        const root = self.state == ParsState.key_root;
        const lbrace_token = token_ptr.* orelse {
            if (!root) {
                return error.EndOfStream;
            } else return Node.newRoot(self.allocator);
        };
        var node = Node.newMap(self.allocator, lbrace_token);
        errdefer node.destroy(self.allocator);
        if (!root) {
            token_ptr.* = lbrace_token.next;
            self.state = ParsState.key;
        }
        while (token_ptr.*) |token| {
            self.last_token = token_ptr.*;
            switch (self.state) {
                .key, .key_root => {
                    if (token.type == TokenType.rbrace and !root) {
                        node.len = token.offset + token.len;
                        return node;
                    } else if (token.type == TokenType.word) {
                        var kv = try self.parseKv(token_ptr);
                        if (!root) {
                            self.state = ParsState.map_delim;
                        } else {
                            self.state = ParsState.map_delim_root;
                        }
                        if (node.data.map.last) |last| {
                            last.next = kv;
                        } else {
                            node.data.map.first = kv;
                        }
                        node.data.map.last = kv;
                    } else try self.err(token);
                },
                .map_delim, .map_delim_root => {
                    if (token.type == TokenType.rbrace and !root) {
                        node.len = token.offset + token.len;
                        return node;
                    } else if (token.type == TokenType.comma) {
                        if (!root) {
                            self.state = ParsState.key;
                        } else {
                            self.state = ParsState.key_root;
                        }
                    } else try self.err(token);
                },
                else => unreachable,
            }
            self.last_token = token_ptr.*;
            var current_token = token_ptr.* orelse break;
            token_ptr.* = current_token.next;
        }
        if (!root) {
            return error.EndOfStream;
        } else return node;
    }

    fn parseArray(self: *Self, token_ptr: *?*Token) ParsErr!*Node {
        const array_token = token_ptr.* orelse return error.EndOfStream;
        var node = Node.newArray(self.allocator, array_token);
        errdefer node.destroy(self.allocator);
        var array = &node.data.array;
        var arr_node_token = token_ptr.* orelse return error.EndOfStream;
        token_ptr.* = arr_node_token.next;
        self.state = ParsState.arr_node;
        while (token_ptr.*) |token| {
            self.last_token = token_ptr.*;
            switch (self.state) {
                .arr_node => {
                    if (token.type == TokenType.rbracket) {
                        node.len = token.offset + token.len;
                        return node;
                    } else if (token.type == TokenType.lbracket) {
                        var map = try self.parseArray(token_ptr);
                        self.state = ParsState.arr_delim;
                        if (array.last) |last| {
                            last.next = map;
                        } else {
                            array.first = map;
                        }
                        array.last = map;
                    } else if (token.type == TokenType.lbrace) {
                        var map = try self.parseMap(token_ptr);
                        self.state = ParsState.arr_delim;
                        if (array.last) |last| {
                            last.next = map;
                        } else {
                            array.first = map;
                        }
                        array.last = map;
                    } else if (token.type == TokenType.word or token.type == TokenType.number) {
                        var value = try self.parseValue(token_ptr);
                        self.state = ParsState.arr_delim;
                        if (array.last) |last| {
                            last.next = value;
                        } else {
                            array.first = value;
                        }
                        array.last = value;
                    } else {
                        try self.err(token);
                    }
                },
                .arr_delim => {
                    if (token.type == TokenType.rbracket) {
                        node.len = token.offset + token.len;
                        return node;
                    } else if (token.type == TokenType.comma) {
                        self.state = ParsState.arr_node;
                    } else try self.err(token);
                },
                else => unreachable,
            }
            self.last_token = token_ptr.*;
            var current_token = token_ptr.* orelse break;
            token_ptr.* = current_token.next;
        }
        return error.EndOfStream;
    }

    pub fn deinit(self: *Self) void {
        if (self.root) |root| {
            root.destroy(self.allocator);
        }
    }

    pub fn print(self: *Self, writer: *std.fs.File.Writer) void {
        if (self.root) |root| {
            writer.print("{s}\n", .{
                NodeFormatter.pretty(root, self.source, 0),
            }) catch unreachable;
        }
    }

    pub fn errorFormatted(self: *const Self, err_value: ParsErr) ParsErrFormatter {
        return ParsErrFormatter{
            .token = (self.last_token orelse &Token{}).*,
            .expected = self.state.expected(),
            .source = self.source,
            .err = err_value,
        };
    }
};

const ParsErrFormatter = struct {
    const Self = @This();

    token: Token,
    expected: []const u8,
    source: []const u8,
    err: Pars.ParsErr,

    pub fn format(
        self: Self,
        comptime fmt: []const u8,
        options: std.fmt.FormatOptions,
        writer: anytype,
    ) !void {
        _ = options;
        if (fmt.len == 1 and fmt[0] == 's') {
            switch (self.err) {
                Pars.ParsErr.InvalidToken => {
                    try writer.print("<stdin>:{d}:{d}: unexpected token {d}, expected {s}", .{
                        self.token.line,
                        self.token.col,
                        TokenFormatter.fromToken(&self.token, self.source),
                        self.expected,
                    });
                },
                Pars.ParsErr.EndOfStream => {
                    try writer.print("<stdin>:{d}:{d}: unexpected EOF, expected {s}", .{
                        self.token.line,
                        self.token.col + self.token.len,
                        self.expected,
                    });
                },
            }
        } else @compileError(
            "Only {s} specifier can be used for " ++ @typeName(Self) ++ " formatting",
        );
    }
};

pub fn main() !void {
    var gpalloc = std.heap.GeneralPurposeAllocator(.{}){};
    defer assert(!gpalloc.deinit());
    const allocator = gpalloc.allocator();
    const reader = &std.io.getStdIn().reader();
    const writer = &std.io.getStdOut().writer();
    _ = writer.write(">") catch unreachable;
    const data = reader.readAllAlloc(allocator, std.math.maxInt(usize)) catch |err| {
        log.err("{s}", .{err});
        return;
    };
    defer allocator.free(data);
    var lex = Lex.init(allocator);
    defer lex.deinit();
    if (!lex.consume(data)) {
        log.err("lexing {s}", .{lex.errorFormatted()});
        return;
    }
    var parser = Pars.init(allocator, lex.source);
    defer parser.deinit();
    parser.parse(lex.first) catch |err| {
        log.err("parsing {s}", .{parser.errorFormatted(err)});
        return;
    };
    parser.print(writer);
}
