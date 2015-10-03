#!/usr/bin/perl

use warnings;
use strict;

if (@ARGV < 2) {
	die <<EOF;
usage: $0 <in> <out> <file>
	Read the <file> and randomly pick <in> lines from it,
	then emit <out> lines of input for Tbl-test.{c,pl}.
EOF
}

my $i = shift;
my $o = shift;

my @p = qw( - + * * * * );
my @i = <>;
my @a;

push @a, splice @i, (int rand @i), 1 while $i--;
print $p[int rand @p], $a[int rand @a] while $o--;
