#!/usr/bin/perl

use warnings;
use strict;

use MIME::Base64;

my @prog;

push @prog, shift while @ARGV and $ARGV[0] ne '--';
if('--' ne shift @ARGV) {
	die <<EOF;
usage: $0 <prog>... -- <args>
EOF
}

my %stats;
my $w = 0;
for (@prog) {
    $w = length if $w < length;
}

open my $rnd, '<', '/dev/urandom'
    or die "open /dev/urandom: $!\n";

for (my $N = 1 ;; ++$N) {
	my $seed;
	sysread $rnd, $seed, 12;
	$seed = encode_base64 $seed, "";

	for my $prog (@prog) {
		print "$prog $seed @ARGV\n";
		for (qx{$prog $seed @ARGV}) {
			if(m{^(\w+)... ([0-9.]+) s$}) {
				my $test = $1;
				my $time = $2;
				$stats{$test}{$prog}{this} = $time;
				$stats{$test}{$prog}{min} = $time
				    if not defined $stats{$test}{$prog}{min}
				    or $stats{$test}{$prog}{min} > $time;
				$stats{$test}{$prog}{tot} += $time;
				$stats{$test}{$prog}{tot2} += $time * $time;
			}
		}
	}

	printf "%-*s", $w, "";
	printf " | %-31s", $_ for sort keys %stats;
	print "\n";
	for my $prog (@prog) {
		printf "%-*s", $w, $prog;
		for my $test (sort keys %stats) {
			my $mean = $stats{$test}{$prog}{tot} / $N;
			my $var = $stats{$test}{$prog}{tot2} / $N - $mean * $mean;
			printf " | %.3f : %.3f < %.3f +/- %.3f",
			    $stats{$test}{$prog}{this},
			    $stats{$test}{$prog}{min},
			    $mean, $var ** 0.5;
		}
		print "\n";
	}

}
