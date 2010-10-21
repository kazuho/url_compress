#! /usr/bin/perl

use strict;
use warnings;

use List::MoreUtils qw/uniq/;

my @cnt = map { 0 } 0..255;

while (my $l = <STDIN>) {
    chomp $l;
    $l =~ s|%([8-9A-Fa-f][0-9A-Fa-f])|chr(hex($1))|eg;
    $l =~ s|%([0-9a-f])([0-9a-f])|'%' . uc($1) . uc($2)|eg;
    foreach my $ch (split '', $l) {
        $cnt[ord $ch]++;
    }
}

my @order = map { ord($_) } split '', "\0/.t";
@order = uniq(
    @order,
    sort { $cnt[$b] <=> $cnt[$a] } 0..255,
);

print "static unsigned char charmap_from_ordered[] = {", join(',', @order), "};\n";

my %r = map { $order[$_] => $_ } @order;
print "static unsigned char charmap_to_ordered[] = {", join(',', map { $r{$_} } 0..255), "};\n";
