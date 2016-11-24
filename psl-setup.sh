#!/bin/sh

[ -f public_suffix_list.dat ] ||
	curl -fO https://publicsuffix.org/list/public_suffix_list.dat

if ! idn=$(type -p idn)
then idn=cat
fi

cat public_suffix_list.dat |
	sed 's| *//.*||;s|^\*\.||' |
	$idn |
	egrep '^[a-z0-9.-]+$' |
	rev |
	env -i LANG=C sort > psl.dat
