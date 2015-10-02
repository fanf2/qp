#!/usr/bin/perl

# like Tbl-test.c but written in perl to verify correctness

my %t;

while(<>) {
	m{^([-+])(.*)$}s or die "bad input line";
	delete $t{$2} if $1 eq '-';
	$t{$2} = 1 if $1 eq '+';
}

print for sort keys %t;
