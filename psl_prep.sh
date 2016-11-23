#!/bin/sh

[ -f public_suffix_list.dat ] ||
	curl -fO https://publicsuffix.org/list/public_suffix_list.dat

cat public_suffix_list.dat |
	sed 's| *//.*||;s|^\*\.||;/^!/d;/^$/d' |
	idn |
	env -i LANG=C sort > psl.dat
