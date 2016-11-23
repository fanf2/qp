#!/usr/bin/perl

use warnings;
use strict;

use Data::Dumper;

our $T;

while (<>) {
	chomp;
	my $e = join '', map "->{$_}", reverse split //;
	eval '$T'.$e.'=1;'
}

sub count {
	my $t = shift;
	if (ref $t) {
		my @k = keys %$t;
		my $sub = 0;
		$sub += count($t->{$_}) for @k;
		if (@k > 1) {
			return 8 + $sub;
		} else {
			return 1 + $sub;
		}
	} else {
		return 1;
	}
}

printf "%d\n", count $T;
