#! /usr/bin/perl

use strict;
use warnings;

while (my $url = <STDIN>) {
    chomp $url;
    # normalize
    if ($url =~ m{^(https?://)([^/:]+(?:\d+|))(/.*|)$}) {
        my ($proto, $hostport, $path) = ($1, $2, $3);
        $hostport = lc $hostport;
        if ($hostport !~ m{^[0-9A-Za-z\.\-_:]+$}) {
            warn "unexpected char in hostport part, skipping URL: $url";
            next;
        }
        $path =~ s/\%([0-9A-Fa-f][0-9A-Fa-f])/'%' . uc $1/eg;
        $path =~ s/([\x80-\xff])/sprintf("%%%02X", ord $1)/eg;
        $path = '/' if $path eq '';
        $url = "$proto$hostport$path";
    }
    print "$url\n";
}
