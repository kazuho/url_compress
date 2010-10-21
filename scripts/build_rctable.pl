#! /usr/bin/perl

use strict;
use warnings;

use Data::Dump qw/dump/;
use List::Util qw/max sum/;

sub typename {
    my $n = shift;
    if ($n < 256) {
        return 'unsigned char';
    } elsif ($n < 65536) {
        return 'unsigned short';
    }
    return 'unsigned int';
}

sub sizeof {
    my $t = shift;
    return 1 if $t =~ /\s?char$/;
    return 2 if $t =~ /\s?short$/;
    return 4;
}

my $PRED_TYPE = 'short'; # int

die "Usage: $0 <data_file> <num_tables> <tbl_sample_ratio> <test_ratio>\n"
    if @ARGV < 4;

my ($data_file, $num_tables, $tbl_sample_ratio, $test_ratio, $mode) = @ARGV;
$mode ||= 'url';

print "#define PRED_BASE ", ($PRED_TYPE eq 'short' ? 'SHRT_MIN' : '0'), "\n";

# load reorder table
my @charmap_to_ordered;
if (1) {
    open my $fh, '<', 'src/chreorder.c' or die 'failed to open chreorder.c';
    while (my $l = <$fh>) {
        chomp $l;
        if ($l =~ / charmap_to_ordered.*?\{(.*?)\}/) {
            @charmap_to_ordered = split /\s*,\s*/, $1;
            last;
        }
    }
    die "failed to parse chreorder.c\n"
        unless scalar(@charmap_to_ordered) == 256;
    close $fh;
    print "#define USE_CHARMAP 1\n"
} else {
    @charmap_to_ordered = 0..255;
}

my @chars_allowed;
my $num_chars_allowed;
my @HC;
if ($mode eq 'host') {
    @HC = sort {
        $a cmp $b
    } map {
        chr $charmap_to_ordered[ord $_]
    } ("\0", '0'..'9', 'a'..'z', qw(- _ : .));
    $chars_allowed[ord $_] = 1
        for grep { $_ ne $charmap_to_ordered[ord '/'] } @HC;
} else {
    @HC = sort {
        $a cmp $b
    } map {
        chr $charmap_to_ordered[ord $_]
    } ("\0", '0'..'9', 'a'..'z', qw(- _ : / .));
    @chars_allowed = map { 1 } 0..255;
}
my %HC;
$HC{ord $HC[$_]} = $_ for 0..$#HC;
$num_chars_allowed = scalar grep { $_ } @chars_allowed;

my @cnt3 = map { [ map { [ map { 1 } 0..$#HC ] } 0..$#HC ] } 0..$#HC;
my @cnt2 = map { [ map { 1 } 0..255 ] } 0..255;
my @cnt1 = map { 1 } 0..255;
my @cnt;
my ($total, $num);

sub build_table {
    my $pred = shift;
    
    for (my $i = 0; $i < @HC; $i++) {
        for (my $j = 0; $j < @HC; $j++) {
            for (my $k = 0; $k < @HC; $k++) {
                $cnt3[$i]->[$j]->[$k] = [ map { 0 } 0..255 ]
                    if $cnt3[$i]->[$j]->[$k];
            }
        }
    }
    for (my $i = 0; $i < 256; $i++) {
        for (my $j = 0; $j < 256; $j++) {
            $cnt2[$i]->[$j] = [ map { 0 } 0..255 ]
                if $cnt2[$i]->[$j];
        }
        $cnt1[$i] = [ map { 0 } 0..255 ]
            if $cnt1[$i];
    }
    @cnt = map { 0 } 0..255;
    
    print STDERR ".";
    
    open my $fd, '<', $data_file
        or die "failed to open: $data_file";
    $total = 0;
    $num = 0;
    my $l = 0;
    while (my $url = <$fd>) {
        $l++;
        if ($pred != 0) {
            next if ($l + 1) % $tbl_sample_ratio;
        } else {
            next if $l % $test_ratio;
        }
        chomp $url;
        $url =~ m|^https?://(.*?)/| or die "unexpected url: $url\n";
        my ($prev, $prev2, $prev3) = (0, 0, 0);
        my $target;
        if ($mode eq 'host') {
            $target = $1;
        } elsif ($mode eq 'path') {
            $target = $';
            $prev = $charmap_to_ordered[ord '/'];
        } else {
            $target = $url;
        }
        $total += length($target);
        $num++;
        $target =~ s|%([8-9A-Fa-f][0-9A-Fa-f])|chr(hex($1))|eg;
        $target =~ s|%([0-9a-f])([0-9a-f])|'%' . uc($1) . uc($2)|eg;
        foreach my $c (split '', $target) {
            my $cc = $charmap_to_ordered[ord $c];
            my $t;
            if ($HC{$prev3} && $HC{$prev2} && $HC{$prev}) {
                $t = $cnt3[$HC{$prev3}]->[$HC{$prev2}]->[$HC{$prev}];
            }
            $t ||= $cnt2[$prev2]->[$prev] || $cnt1[$prev] || \@cnt;
            $t->[$cc]++;
            $prev3 = $prev2;
            $prev2 = $prev;
            $prev = $cc;
        }
        my $t;
        if ($HC{$prev3} && $HC{$prev2} && $HC{$prev}) {
            $t = $cnt3[$HC{$prev3}]->[$HC{$prev2}]->[$HC{$prev}];
        }
        $t ||= $cnt2[$prev2]->[$prev] || $cnt1[$prev] || \@cnt;
        $t->[0]++;
    }
    close $fd;
    
    print STDERR ",";
}

sub add_to_active {
    my ($l, $m, $ctx) = @_;
    my $s = sum @$m
        or return;
    push @$l, {
        map => $m,
        cnt => $s,
        ctx => $ctx,
    };
}

# build 3-byte pred table and take the most used NUM_TABLES maps
build_table(3);
my @active;
my %active;
for (my $i = 0; $i < @HC; $i++) {
    for (my $j = 0; $j < @HC; $j++) {
        for (my $k = 0; $k < @HC; $k++) {
            add_to_active(\@active, $cnt3[$i]->[$j]->[$k], "$i,$j,$k");
        }
    }
}
@active = sort { $b->{cnt} <=> $a->{cnt} } @active;
die "not enough data\n" if scalar(@active) < $num_tables;
%active = map { $_->{ctx} => 1 } @active;
for (my $i = 0; $i < @HC; $i++) {
    for (my $j = 0; $j < @HC; $j++) {
        for (my $k = 0; $k < @HC; $k++) {
            unless ($active{"$i,$j,$k"}) {
                delete $cnt3[$i]->[$j]->[$k];
            }
        }
    }
}

# build 2-byte pred table and take the most used NUM_TABLES maps
for (my $loop = 0; $ loop < 3; $loop++) {
    @cnt2 = map { [ map { 1 } 0..255 ] } 0..255;
    build_table(2);
    @active = ();
    for (my $i = 0; $i < @HC; $i++) {
        for (my $j = 0; $j < @HC; $j++) {
            for (my $k = 0; $k < @HC; $k++) {
                add_to_active(\@active, $cnt3[$i]->[$j]->[$k], "$i,$j,$k")
                    if $cnt3[$i]->[$j]->[$k];
            }
        }
    }
    for (my $i = 0; $i < 256; $i++) {
        for (my $j = 0; $j < 256; $j++) {
            add_to_active(\@active, $cnt2[$i]->[$j], "$i,$j");
        }
    }
    @active = sort { $b->{cnt} <=> $a->{cnt} } @active;
    splice @active, $num_tables;
    %active = map { $_->{ctx} => 1 } @active;
    for (my $i = 0; $i < @HC; $i++) {
        for (my $j = 0; $j < @HC; $j++) {
            for (my $k = 0; $k < @HC; $k++) {
                unless ($active{"$i,$j,$k"}) {
                    delete $cnt3[$i]->[$j]->[$k]
                        if $cnt3[$i]->[$j]->[$k];
                }
            }
        }
    }
    for (my $i = 0; $i < 256; $i++) {
        for (my $j = 0; $j < 256; $j++) {
            if ($active{"$i,$j"}) {
                # printf "saving: %c%c\n", $i, $j;
            } else {
                delete $cnt2[$i]->[$j];
            }
        }
    }
}

# build 1-byte pred table and take the most used NUM_TABLES maps from cnt1 and cnt2
for (my $loop = 0; $loop < 3; $loop++) {
    @cnt1 = map { 1 } 0..255;
    build_table(1);
    @active = ();
    for (my $i = 0; $i < @HC; $i++) {
        for (my $j = 0; $j < @HC; $j++) {
            for (my $k = 0; $k < @HC; $k++) {
                add_to_active(\@active, $cnt3[$i]->[$j]->[$k], "$i,$j,$k");
            }
        }
    }
    for (my $i = 0; $i < 256; $i++) {
        add_to_active(\@active, $cnt1[$i], $i);
        for (my $j = 0; $j < 256; $j++) {
            add_to_active(\@active, $cnt2[$i]->[$j], "$i,$j")
                if $cnt2[$i]->[$j];
        }
    }
    @active = sort { $b->{cnt} <=> $a->{cnt} } @active;
    splice @active, $num_tables;
    %active = map { $_->{ctx} => $_ } @active;
    for (my $i = 0; $i < @HC; $i++) {
        for (my $j = 0; $j < @HC; $j++) {
            for (my $k = 0; $k < @HC; $k++) {
                unless ($active{"$i,$j,$k"}) {
                    delete $cnt3[$i]->[$j]->[$k]
                        if $cnt3[$i]->[$j]->[$k];
                }
            }
        }
    }
    for (my $i = 0; $i < 256; $i++) {
        for (my $j = 0; $j < 256; $j++) {
            unless ($active{"$i,$j"}) {
                delete $cnt2[$i]->[$j]
                    if $cnt2[$i]->[$j];
            }
        }
        unless ($active{$i}) {
            delete $cnt1[$i];
        }
    }
}

# build no-pred table
build_table(0);

my @maps;
my ($npred1, $npred2, $npred3) = (0, 0, 0);

sub build_map {
    my ($cnt, $backup) = @_;
    my $unq = scalar grep { $_ != 0 } @$cnt;
    my ($backup_sum, $backup_max) = (0, 0);
    if ($backup) {
        for (my $i = 0; $i < 256; $i++) {
            if (! $cnt->[$i] && $chars_allowed[$i]) {
                $backup_sum += $backup->[$i + 1] - $backup->[$i];
                $backup_max =
                    max($backup_max, $backup->[$i + 1] - $backup->[$i]);
            }
        }
    }
    my $sum = sum @$cnt;
    my $esc_total = 0;
    if ($num_chars_allowed != $unq) {
        if ($backup) {
            $esc_total = 0.8 / ($sum + 1) * $backup_sum / $backup_max;
        } else {
            $esc_total = 0.1 / ($sum + 1) * ($num_chars_allowed - $unq);
        }
    }
    my $esc_mult = $esc_total * $sum / ($sum + $esc_total * $sum);
    $sum += $esc_total * $sum;
    my $f = sub {
        my $freq_mult = shift;
        my @map;
        my $acc = 0;
        for (my $i = 0; $i < 256; $i++) {
            my $p;
            if ($chars_allowed[$i]) {
                if ($cnt->[$i]) {
                    $p = $cnt->[$i] / $sum * $freq_mult;
                } elsif ($backup) {
                    $p = ($backup->[$i + 1] - $backup->[$i]) / $backup_sum
                        * $esc_mult * $freq_mult;
                } else {
                    $p = $esc_mult / ($num_chars_allowed - $unq) * $freq_mult;
                }
            if ($p < 1) {
                $p = 1;
            } else {
                $p = int($p + 0.5);
            }
            } else {
                $p = 0;
            }
            push @map, $acc;
            $acc += $p;
        }
        push @map, $acc;
        \@map;
    };
    #print STDERR ".";
    my $freq_exp = $PRED_TYPE eq 'short' ? 0x8000 : 0x800000;
    my ($l, $r, $m, $lval, $rval) = (0, 0, $freq_exp, 0, 0);
    while (1) {
        my $map = $f->($m);
        # print STDERR "($l:$lval,$r:$rval,$m:$map->[-1])";
        if ($map->[-1] == $freq_exp) {
            # print STDERR "=\n";
            return $map;
        }
        if ($map->[-1] > $freq_exp) {
            ($r, $rval) = ($m, $map->[-1]);
            $m = $l ? ($l + $r) / 2 : $m - 50;
        } else {
            ($l, $lval) = ($m, $map->[-1]);
            $m = $r ? ($l + $r) / 2 : $m + 50;
        }
        if ($lval == $rval || abs($r - $l) < 0.5) {
            # print STDERR "+\n";
            my $d = $freq_exp - $total;
            my @idx = sort {
                $map->[$b+1]-$map->[$b] <=> $map->[$a+1]-$map->[$a]
            } 0..255;
            while ($freq_exp != $map->[-1]) {
                my $d = $map->[-1] < $freq_exp ? 1 : -1;
                for (my $i = shift(@idx) + 1; $i < 257; $i++) {
                    $map->[$i] += $d;
                }
            }
            return $map;
        }
    }
}

push @maps, build_map(\@cnt);

my @p1idx;
for (my $i = 0; $i < 256; $i++) {
    if (my $c = $cnt1[$i]) {
        $p1idx[$i] = scalar @maps;
        push @maps, build_map($c, $maps[0]);
        $npred1++;
    } else {
        $p1idx[$i] = 0;
    }
}

my @p2idx = map { [] } 0..255;
for (my $i = 0; $i < 256; $i++) {
    for (my $k = 0; $k < 256; $k++) {
        if (my $c = $cnt2[$i]->[$k]) {
            $p2idx[$i]->[$k] = scalar @maps;
            push @maps, build_map($c, $maps[$p1idx[$k]]);
            $npred2++;
        } else {
            $p2idx[$i]->[$k] = $p1idx[$k];
        }
    }
}

my @p3idx = map { [ map { [] } 0..255 ] } 0..255;
for (my $i = 0; $i < 256; $i++) {
    for (my $j = 0; $j < 256; $j++) {
        for (my $k = 0; $k < 256; $k++) {
            my $c;
            if (defined $HC{$i} && defined $HC{$j} && defined $HC{$k}
                    && ($c = $cnt3[$HC{$i}]->[$HC{$j}]->[$HC{$k}])) {
                $p3idx[$i]->[$j]->[$k] = scalar @maps;
                push @maps, build_map($c, $maps[$p2idx[$j]->[$k]]);
                $npred3++;
            } else {
                $p3idx[$i]->[$j]->[$k] = $p2idx[$j]->[$k];
            }
        }
    }
}

print STDERR "level: 1,$npred1,$npred2,$npred3\n";

my $prefix;
if ($mode eq 'host') {
    $prefix = 'host_';
} elsif ($mode eq 'path') {
    $prefix = 'path_';
} else {
    $prefix = '';
}

sub to_s {
    my $v = shift;
    $v = unpack('H*', pack 'N', $v);
    $v =~ s/../\\x$&/g;
    $v;
}

my (@predidx1, @predidx2, @predidx3);
for (my $k = 0; $k < 256; $k++) {
    my $comp = $p3idx[0]->[0]->[$k];
    for (my $j = 0; $j < 256 && $comp != -1; $j++) {
        for (my $i = 0; $i < 256; $i++) {
            if ($p3idx[$i]->[$j]->[$k] != $comp) {
                $comp = -1;
                last;
            }
        }
    }
    if ($comp != -1) {
        push @predidx1, $comp;
        next;
    }
    push @predidx1, scalar(@predidx2) + $num_tables + 1;
    for (my $j = 0; $j < 256; $j++) {
        my $comp2 = $p3idx[0]->[$j]->[$k];
        for (my $i = 0; $i < 256; $i++) {
            if ($p3idx[$i]->[$j]->[$k] != $comp2) {
                $comp2 = -1;
                last;
            }
        }
        if ($comp2 != -1) {
            push @predidx2, $comp2;
            next;
        }
        push @predidx2, scalar(@predidx3) + $num_tables + 1;
        for (my $i = 0; $i < 256; $i++) {
            push @predidx3, $p3idx[$i]->[$j]->[$k];
        }
    }
}

my $predidx1_t = typename(scalar(@predidx2) + $num_tables + 1);
my $predidx2_t = typename(scalar(@predidx3) + $num_tables + 1);
my $predidx3_t = typename($num_tables + 1);

if ($mode eq 'host') {
    print "#define NUM_HOST_PRED_MAPS ", ($num_tables + 1), "\n";
} elsif ($mode eq 'path') {
    print "#define NUM_PATH_PRED_MAPS ", ($num_tables + 1), "\n";
} else {
    print "#define NUM_PRED_MAPS ", ($num_tables + 1), "\n";
}
print "typedef $PRED_TYPE pred_map_t[264];\n";
print "#define PRED_MAX ", ($PRED_TYPE eq 'short' ? '0x8000' : '0x800000'), "\n";
print "static $predidx1_t ${prefix}predidx1[] = {", join(',', @predidx1), "};\n";
print "static $predidx2_t ${prefix}predidx2[] = {", join(',', @predidx2), "};\n";
print "static $predidx3_t ${prefix}predidx3[] = {", join(',', @predidx3), "};\n";
print "static pred_map_t ${prefix}predmaps[] __attribute__((aligned(16))) = {\n", join(",\n", map { '{' . join(',', map { $PRED_TYPE eq 'short' ? $_ - 0x8000 : $_ } @$_) . '}' } @maps), "\n};\n";

print STDERR "Index size: ", scalar(@predidx1) * sizeof($predidx1_t) + scalar(@predidx2) * sizeof($predidx2_t) + scalar(@predidx3) * sizeof($predidx3_t) + ($num_tables + 1) * 264 * sizeof($PRED_TYPE), "\n";
