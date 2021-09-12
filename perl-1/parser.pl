package Assert;

use strict;
use warnings;

sub assert {
    my ($condition, $msg) = @_;
    return if $condition;
    if (!$msg) {
        my ($pkg, $file, $line) = caller(0);
        open my $fh, "<", $file;
        my @lines = <$fh>;
        close $fh;
        $msg = "$file:$line: " . $lines[$line - 1];
    }
    die "Assertion failed: $msg";
}
1;

package Tokenizer;

use strict;
use warnings;

use constant {
	s_idle => 'idle',
	s_word => 'matching word',
	s_number => 'matching number',
	t_delim => '[\{\}=,]',
	t_space => '[\s]',
	t_number_begin => '[0-9]',
	t_number_cont => '[0-9xa-fA-F]',
	t_word_begin => '[a-zA-Z_]',
	t_word_cont => '[a-zA-Z0-9_]',
};

sub new {
	my $class = shift;
	my $self = {
		val => 10,
		tokens => [],
		state => s_idle,
		number => "",
		word => "",
	};
	bless $self, $class;
	return $self;
}

sub error {
	my $t = shift;
	my $state = shift;
	my @tokens = @_;
	print "@tokens\n";
	my $tokens_string = join '|', @tokens;
	print "Unexpected symbol \'$t\' at state \'$state\', " .
		"expected: $tokens_string\n";
	exit 1;
}

sub consume_number {
	my $self = shift;
	my $sym = shift;
	if ($sym =~ /${\t_number_cont}/) {
		$self->{number} .= $sym;
	} else {
		push @{ $self->{tokens} }, $self->{number};
		$self->{number} = "";
		if ($sym =~ /${\t_word_begin}/) {
			$self->{state} = s_word;
			$self->{word} .= $sym;
		} elsif ($sym =~ /${\t_delim}/) {
			$self->{state} = s_idle;
			push @{ $self->{tokens} }, $sym;
		} elsif ($sym =~ /${\t_space}/) {
			$self->{state} = s_idle;
			#ignore
		} else {
			error $sym, $self->{state}, (
				t_number_cont,
				t_word_begin,
				t_delim,
				t_space,
			);
		}
	}
}

sub consume_word {
	my $self = shift;
	my $sym = shift;
	if ($sym =~ /${\t_word_cont}/) {
		$self->{word} .= $sym;
	} else {
		push @{ $self->{tokens} }, $self->{word};
		$self->{word} = "";
		if ($sym =~ /${\t_number_begin}/) {
			$self->{state} = s_word;
			$self->{number} .= $sym;
		} elsif ($sym =~ /${\t_delim}/) {
			$self->{state} = s_idle;
			push @{ $self->{tokens} }, $sym;
		} elsif ($sym =~ /${\t_space}/) {
			$self->{state} = s_idle;
			#ignore
		} else {
			error $sym, $self->{state}, (
				t_number_cont,
				t_word_begin,
				t_delim,
				t_space,
			);
		}
	}
}

sub consume_idle {
	my $self = shift;
	my $sym = shift;
	if ($sym =~ /${\t_number_begin}/) {
		$self->{state} = s_number;
		$self->{number} .= $sym;
	} elsif ($sym =~ /${\t_word_begin}/) {
		$self->{state} = s_word;
		$self->{word} .= $sym;
	} elsif ($sym =~ /${\t_delim}/) {
		push @{ $self->{tokens} }, $sym;
	} elsif ($sym =~ /${\t_space}/) {
		#ignore
	} else {
		error $sym, $self->{state}, (
			t_number_begin,
			t_word_begin,
			t_delim,
			t_space,
		);
	}
}

sub consume_sym {
	my $self = shift;
	my $sym = shift;
	if ($self->{state} eq s_number) {
		$self->consume_number($sym);
	} elsif ($self->{state} eq s_word) {
		$self->consume_word($sym);
	} else {
		$self->consume_idle($sym);
	}
}

sub consume {
	my $self = shift;
	my $line = shift;
	foreach my $sym (split //, $line) {
		$self->consume_sym($sym);
	}
}
1;

package Parser;

use strict;
use warnings;

use constant {
	s_word => '<word>',
	s_word_or_close => '<word> or `}`',
	s_assign => '`=`',
	s_number_or_open => '<number>',
	s_delim_or_close => '`,` or `}`',
	s_delim => '`,`',
	t_number => '^(0x[0-9a-fA-F]+|0b[0-1]+|[0-9]+)$',
	t_word => '^[a-zA-Z_][a-zA-Z0-9_]*$',
	t_assign => '^=$',
	t_delim => '^,$',
	t_open => '^{$',
	t_close => '^}$',
};

sub new {
	my $class = shift;
	my $self = {
		val => 10,
		tree => [],
		stack => [],
		state => s_word,
	};
	bless $self, $class;
	return $self;
}

sub err_token {
	my $t = shift;
	my $expected = shift;
	print "Unexpected token `$t`, expected $expected\n";
	exit 1;
}

sub act_close {
	my $self = shift;
	my @pair = @{ pop @{ $self->{stack} } };
	push @pair, $self->{tree};
	my @tree = @{ pop @{ $self->{stack} } };
	push @tree, [ @pair ];
	$self->{tree} = \@tree;
	$self->{pair} = [];
	if (scalar $self->{stack} <= 0) {
		assert(scalar $self->{stack} == 0);
		$self->{state} = s_delim;
	} else {
		$self->{state} = s_delim_or_close;
	}
}

sub word {
	my $self = shift;
	my $token = shift;
	if ($token =~ /${\t_word}/) {
		push @{ $self->{pair} }, $token;
		$self->{state} = s_assign;
	} else {
		err_token $token, s_word;
	}
}

sub word_or_close {
	my $self = shift;
	my $token = shift;
	if ($token =~ /${\t_word}/) {
		push @{ $self->{pair} }, $token;
		$self->{state} = s_assign;
	} elsif ($token =~ /${\t_close}/) {
		$self->act_close();
	} else {
		err_token $token, s_word_or_close;
	}
}

sub assign {
	my $self = shift;
	my $token = shift;
	if ($token =~ /${\t_assign}/) {
		$self->{state} = s_number_or_open;
	} else {
		err_token $token, s_assign;
	}
}

sub number_or_open {
	my $self = shift;
	my $token = shift;
	if ($token =~ /${\t_number}/) {
		push @{ $self->{pair} }, $token;
		push @{ $self->{tree} }, $self->{pair};
		$self->{pair} = [];
		if (scalar $self->{stack} <= 0) {
			assert(scalar $self->{stack} == 0);
			$self->{state} = s_delim;
		} else {
			$self->{state} = s_delim_or_close;
		}
	} elsif ($token =~ /${\t_open}/) {
		push @{ $self->{stack} }, $self->{tree};
		push @{ $self->{stack} }, $self->{pair};
		$self->{tree} = [];
		$self->{pair} = [];
		$self->{state} = s_word;
	} else {
		err_token $token, s_number_or_open;
	}
}

sub delim {
	my $self = shift;
	my $token = shift;
	if ($token =~ /${\t_delim}/) {
		$self->{state} = s_word;
	} else {
		err_token $token, s_delim;
	}
}

sub delim_or_close {
	my $self = shift;
	my $token = shift;
	if ($token =~ /${\t_delim}/) {
		$self->{state} = s_word_or_close;
	} elsif ($token =~ /${\t_close}/) {
		$self->act_close();
	} else {
		err_token $token, s_delim_or_close;
	}
}

sub consume_token {
	my $self = shift;
	my $token = shift;
	if ($self->{state} eq s_word) {
		$self->word($token);
	} elsif ($self->{state} eq s_word_or_close) {
		$self->word_or_close($token);
	} elsif ($self->{state} eq s_assign) {
		$self->assign($token);
	} elsif ($self->{state} eq s_number_or_open) {
		$self->number_or_open($token);
	} elsif ($self->{state} eq s_delim) {
		$self->delim($token);
	} elsif ($self->{state} eq s_delim_or_close) {
		$self->delim_or_close($token);
	} else {
		print "Invalid parser state when expected \"$self->{state}\"";
		exit 1;
	}
}

sub consume {
	my $self = shift;
	my @tokens = @_;
	foreach my $token (@tokens) {
		$self->consume_token($token);
	}
}
1;

package main;

use strict;
use warnings;
use Data::Dumper;

my $tokenizer = Tokenizer->new();
my $parser = Parser->new();
while (my $line = <>) {
	$tokenizer->consume($line);
}
my @tokens = @{ $tokenizer->{tokens} };
print Dumper @tokens; 
$parser->consume(@tokens);
my @tree = $parser->{tree};
print Dumper @tree;
