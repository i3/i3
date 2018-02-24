#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
# renders the layout tree using asymptote
#
# ./dump-asy.pl
#   will render the entire tree
# ./dump-asy.pl 'name'
#   will render the tree starting from the node with the specified name,
#   e.g. ./dump-asy.pl 2 will render workspace 2 and below

use strict;
use warnings;
use Data::Dumper;
use AnyEvent::I3;
use File::Temp;
use File::Basename;
use v5.10;
use IPC::Cmd qw[can_run];

# prerequisites check so we can be specific about failures caused
# by not having these tools in the path
can_run('asy') or die 'Please install asymptote';
can_run('gv') or die 'Please install gv';

my $i3 = i3();

my $tree = $i3->get_tree->recv;

my $tmp = File::Temp->new(UNLINK => 0, SUFFIX => '.asy');

say $tmp "import drawtree;";

say $tmp "treeLevelStep = 2cm;";

sub dump_node {
	my ($n, $parent) = @_;

    my $o = ($n->{orientation} eq 'none' ? "u" : ($n->{orientation} eq 'horizontal' ? "h" : "v"));
    my $w = (defined($n->{window}) ? $n->{window} : "N");
    my $na = ($n->{name} or "[Empty]");
    $na =~ s/#/\\#/g;
    $na =~ s/\$/\\\$/g;
    $na =~ s/&/\\&/g;
    $na =~ s/_/\\_/g;
    $na =~ s/~/\\textasciitilde{}/g;
    my $type = 'leaf';
    if (!defined($n->{window})) {
        $type = $n->{layout};
    }
    my $name = qq|``$na'' ($type)|;

    print $tmp "TreeNode n" . $n->{id} . " = makeNode(";

    print $tmp "n" . $parent->{id} . ", " if defined($parent);
    print $tmp "\"" . $name . "\");\n";

	dump_node($_, $n) for @{$n->{nodes}};
}

sub find_node_with_name {
    my ($node, $name) = @_;

    return $node if ($node->{name} eq $name);
    for my $child (@{$node->{nodes}}) {
        my $res = find_node_with_name($child, $name);
        return $res if defined($res);
    }
    return undef;
}

my $start = shift;
my $root;
if ($start) {
    # Find the specified node in the tree
    $root = find_node_with_name($tree, $start);
} else {
    $root = $tree;
}
dump_node($root);
say $tmp "draw(n" . $root->{id} . ", (0, 0));";

close($tmp);
my $rep = "$tmp";
$rep =~ s/asy$/eps/;
my $tmp_dir = dirname($rep);
system("cd $tmp_dir && asy $tmp && gv --scale=-1000 --noresize --widgetless $rep && rm $rep");
