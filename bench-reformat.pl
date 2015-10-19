#!/usr/bin/perl

use warnings;
use strict;

my @test = <> =~ m{(?:[|]\s+(\w+)\s+)}g;
#printf "tests: %s\n", join " ", @test;

my @file = split m{\s+[|]\s+}, scalar <>;
die "garbage at start of file list"
    unless "" eq shift @file;
chomp $file[-1];
for my $file (@file) {
	die "mismatched file list: <$file> / <$file[-1]>"
	    unless $file eq $file[-1];
}
@file = split ' ', $file[-1];
shift @file if $file[0] eq "";
#printf "files: %s\n", join " ", @file;

my %stats;
my @prog;

while (<>) {
	s{^\s*(\S+)\s+[|]}{|} or die "missing progname";
	my $prog = $1;
	$prog =~ s{^\./bench-}{};
	push @prog, $prog;
	for my $test (@test) {
		s{^\s*[|]\s+}{} or die "missing separator";
		for my $file (@file) {
			s{^\s*([0-9.]+)\s+}{} or die "missing number";
			$stats{$test}{$file}{$prog} = $1;
		}
	}
}

my %min;
for my $test (@test) {
	for my $file (@file) {
		my $min = 99;
		for my $prog (@prog) {
			if ($min > $stats{$test}{$file}{$prog}) {
				$min = $stats{$test}{$file}{$prog};
				$min{$test}{$file} = $prog;
			}
		}
	}
}

print "<table>\n";
print "<tr><th></th>";
print "<th>$_</th>" for @file;
print "<th></th></tr>\n";
for my $test (@test) {
	print "<tr class='break'></tr>\n";
	my $tt = $test;
	for my $prog (@prog) {
		print "<tr><th>$prog</th>";
		for my $file (@file) {
			if ($min{$test}{$file} eq $prog) {
				print "<td><b>$stats{$test}{$file}{$prog}</b></td>";
			} else {
				print "<td>$stats{$test}{$file}{$prog}</td>";
			}
		}
		if ($tt ne "") {
			print "<th class='rightlabel'>$tt</th></tr>\n";
			$tt = "";
		} else {
			print "</tr>\n";
		}
	}
}
print "</table>\n";
