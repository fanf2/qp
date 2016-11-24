#!/usr/bin/perl

use warnings;
use strict;

use Data::Dumper;

our $T;

while (<>) {
	chomp;
	my $e = join '', map "->{'$_'}", reverse split //;
	eval '$T'.$e.'={};';
}

print "$. lines\n";

sub count {
	my $t = shift;
	my @k = keys %$t;
	my ($n,$s) = (0,0);
	for my $k (@k) {
		my ($N,$S) = count($t->{$k});
		$n += $N;
		$s += $S;
	}
	if (@k == 0) {
		return (0,1);
	} elsif ($n == 0 && @k == 1) {
		return (0,$s+1);
	} else {
		return ($n+1,$s);
	}
}
my ($n,$s) = count($T);

printf "%u nodes, %u string, %u bytes\n",
    $n, $s, $n * 7 + $s;
