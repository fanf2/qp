#!/usr/bin/perl

use warnings;
use strict;

use MIME::Base64;

sub maxlen {
	return (sort { $a <=> $b } map { length } @_)[-1];
}

sub usage {
	die <<EOF;
usage: $0 <count> <prog>... -- <input>...
EOF
}

usage if @ARGV < 4 or $ARGV[0] !~ m{^\d+$};
my $count = shift;

my @prog;

push @prog, shift while @ARGV and $ARGV[0] ne '--';
usage if '--' ne shift @ARGV;

my @file = @ARGV;

my $wp = maxlen @prog;
my $wf = maxlen @file, "0.000";
my $waf = ($wf+1) * scalar @file;

my %stats;

open my $rnd, '<', '/dev/urandom'
    or die "open /dev/urandom: $!\n";

for (my $N = 1 ;; ++$N) {
	my $seed;
	sysread $rnd, $seed, 12;
	$seed = encode_base64 $seed, "";

	for my $file (@file) {
		for my $prog (@prog) {
			print "$prog $seed $count $file\n";
			for (qx{$prog $seed $count $file}) {
				if(m{^(\w+)... ([0-9.]+) s$}) {
					my $test = $1;
					my $time = $2;
					$stats{$test}{$prog}{$file}{this} = $time;
					$stats{$test}{$prog}{$file}{min} = $time
					    if not defined $stats{$test}{$prog}{$file}{min}
						or $stats{$test}{$prog}{$file}{min} > $time;
					$stats{$test}{$prog}{$file}{tot} += $time;
					$stats{$test}{$prog}{$file}{tot2} += $time * $time;
				}
			}
		}
	}

	printf "%-*s ", $wp, "";
	printf "| %-*s", $waf, $_ for sort keys %stats;
	print "\n";
	printf "%-*s", $wp, "";
	for (sort keys %stats) {
		printf " |";
		printf " %*s", $wf, $_ for @file;
	}
	print "\n";
	for my $prog (@prog) {
		printf "%-*s", $wp, $prog;
		for my $test (sort keys %stats) {
			printf " |";
			for my $file (@file) {
				my $mean = $stats{$test}{$prog}{$file}{tot} / $N;
				printf " %*.3f", $wf, $mean;
			}
		}
		print "\n";
	}

}
