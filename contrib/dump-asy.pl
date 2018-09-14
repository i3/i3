#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab

use strict;
use warnings;
use Data::Dumper;
use Getopt::Long;
use Pod::Usage;
use AnyEvent::I3;
use File::Temp;
use File::Spec;
use File::Basename;
use v5.10;
use IPC::Cmd qw[can_run];

my %options = (
    gv => 1,
    save => undef,
    help => 0,
);
my $result = GetOptions(
    "gv!" => \$options{gv},
    "save=s" => \$options{save},
    "help|?" => \$options{help},
);

pod2usage(-verbose => 2, -exitcode => 0) if $options{help};

# prerequisites check so we can be specific about failures caused
# by not having these tools in the path
can_run('asy') or die 'Please install asymptote';
can_run('gv') or die 'Please install gv' unless !$options{gv};

my $i3 = i3();

my $tree = $i3->get_tree->recv;

my $tmp = File::Temp->new(UNLINK => 0, SUFFIX => '.asy');

say $tmp "import drawtree;";

say $tmp "treeLevelStep = 2cm;";

sub dump_node {
	my ($n, $parent) = @_;

    my $o = ($n->{orientation} eq 'none' ? "u" : ($n->{orientation} eq 'horizontal' ? "h" : "v"));
    my $w = (defined($n->{window}) ? $n->{window} : "N");
    my $na = ($n->{name} or ($n->{type} eq "floating_con" ? "[Floating con]" : "[Empty]"));
    $na =~ s/#/\\#/g;
    $na =~ s/\$/\\\$/g;
    $na =~ s/&/\\&/g;
    $na =~ s/_/\\_/g;
    $na =~ s/~/\\textasciitilde{}/g;
    my $type = 'leaf';
    if (!defined($n->{window})) {
        $type = $n->{layout};
    }
    my $marks = $n->{marks} ? ' [' . join('][', @{$n->{marks}}) . ']' : '';
    my $name = qq|``$na'' ($type)$marks|;

    print $tmp "TreeNode n" . $n->{id} . " = makeNode(";

    print $tmp "n" . $parent->{id} . ", " if defined($parent);
    print $tmp "\"" . $name . "\");\n";

    dump_node($_, $n) for @{$n->{nodes}};
    dump_node($_, $n) for @{$n->{floating_nodes}};
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
if ($options{gv}) {
    my $tmp_dir = dirname($rep);
    $options{save} = File::Spec->rel2abs($options{save}) if $options{save};
    chdir($tmp_dir);
} else {
    $rep = basename($rep);  # Output in current dir.
}
system("asy $tmp");  # Create the .eps file.
system("gv --scale=-1000 --noresize --widgetless $rep") if $options{gv};
if ($options{save}) {
    system("mv $rep ${options{save}}");
} elsif ($options{gv}) {
    system("rm $rep");
}
system("rm $tmp");

__END__

=head1 NAME

dump-asy.pl - Render the layout tree using asymptote

=head1 SYNOPSIS

dump-asy.pl [workspace]

=head1 EXAMPLE

Render the entire tree, run:

  ./dump-asy.pl

Render the tree starting from the node with the specified name, run:

  ./dump-asy.pl 'name'

Render the entire tree, save in file 'file.eps' and show using gv, run:

  ./dump-asy.pl --save file.eps

=head1 OPTIONS

=over 8

=item B<--gv>

=item B<--no-gv>

Enable or disable showing the result through gv. If disabled, an .eps file will
be saved in the current working directory. Enabled by default.

=item B<--save>

Save result using the specified file-name. If omitted, no file will be saved
when using '--gv' or a random name will be used when using '--no-gv'.

=back
