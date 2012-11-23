#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# renders the layout tree using asymptote

use strict;
use warnings;
use Data::Dumper;
use AnyEvent::I3;
use File::Temp;
use v5.10;

my $i3 = i3();

my $tree = $i3->get_tree->recv;

my $tmp = File::Temp->new(UNLINK => 0, SUFFIX => '.asy');

say $tmp "import drawtree;";

say $tmp "treeLevelStep = 2cm;";

sub dump_node {
	my ($n, $parent) = @_;

    my $o = ($n->{orientation} eq 'none' ? "u" : ($n->{orientation} eq 'horizontal' ? "h" : "v"));
    my $w = (defined($n->{window}) ? $n->{window} : "N");
    my $na = $n->{name};
    $na =~ s/#/\\#/g;
    my $name = "($na, $o, $w)";

    print $tmp "TreeNode n" . $n->{id} . " = makeNode(";

    print $tmp "n" . $parent->{id} . ", " if defined($parent);
    print $tmp "\"" . $name . "\");\n";

	dump_node($_, $n) for @{$n->{nodes}};
}

dump_node($tree);
say $tmp "draw(n" . $tree->{id} . ", (0, 0));";

close($tmp);
my $rep = "$tmp";
$rep =~ s/asy$/eps/;
system("cd /tmp && asy $tmp && gv $rep && rm $rep");
