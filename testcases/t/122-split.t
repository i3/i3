#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • http://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • http://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • http://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests splitting
#
use i3test;
use List::Util qw(first);

my $tmp;
my $ws;

################################################################################
# Open two containers, split, open another container. Then verify
# the layout is like we expect it to be
################################################################################

sub verify_split_layout {
    my (%args) = @_;

    $tmp = fresh_workspace;

    $ws = get_ws($tmp);
    is($ws->{layout}, 'splith', 'orientation horizontal by default');
    cmd 'split v';
    $ws = get_ws($tmp);
    is($ws->{layout}, 'splitv', 'split v changes workspace orientation');

    cmd 'open';
    cmd 'open';
    my $content = get_ws_content($tmp);

    is(@{$content}, 2, 'two containers on workspace level');
    my $first = $content->[0];
    my $second = $content->[1];

    is(@{$first->{nodes}}, 0, 'first container has no children');
    is(@{$second->{nodes}}, 0, 'second container has no children (yet)');
    my $old_id = $second->{id};

    cmd $args{split_command};
    cmd 'open';

    $content = get_ws_content($tmp);

    is(@{$content}, 2, 'two containers on workspace level');
    $first = $content->[0];
    $second = $content->[1];

    is(@{$first->{nodes}}, 0, 'first container has no children');
    isnt($second->{id}, $old_id, 'second container was replaced');
    is($second->{layout}, 'splith', 'orientation is horizontal');
    is(@{$second->{nodes}}, 2, 'second container has 2 children');
    is($second->{nodes}->[0]->{id}, $old_id, 'found old second container');
}

verify_split_layout(split_command => 'split h');
verify_split_layout(split_command => 'split horizontal');

# TODO: extend this test-case (test next/prev)
# - wrapping (no horizontal switch possible, goes level-up)
# - going level-up "manually"

######################################################################
# Test splitting multiple times without actually creating windows
######################################################################

$tmp = fresh_workspace;

$ws = get_ws($tmp);
is($ws->{layout}, 'splith', 'orientation horizontal by default');
cmd 'split v';
$ws = get_ws($tmp);
is($ws->{layout}, 'splitv', 'split v changes workspace orientation');

cmd 'open';
my @content = @{get_ws_content($tmp)};

# recursively sums up all nodes and their children
sub sum_nodes {
    my ($nodes) = @_;

    return 0 if !@{$nodes};

    my @children = (map { @{$_->{nodes}} } @{$nodes},
                    map { @{$_->{'floating_nodes'}} } @{$nodes});

    return @{$nodes} + sum_nodes(\@children);
}

my $old_count = sum_nodes(\@content);
cmd 'split v';

@content = @{get_ws_content($tmp)};
$old_count = sum_nodes(\@content);

cmd 'split v';

@content = @{get_ws_content($tmp)};
my $count = sum_nodes(\@content);
is($count, $old_count, 'not more windows after splitting again');

######################################################################
# In the special case of being inside a stacked or tabbed container, we don’t
# want this to happen.
######################################################################

$tmp = fresh_workspace;

cmd 'open';
@content = @{get_ws_content($tmp)};
is(scalar @content, 1, 'Precisely one container on this ws');
cmd 'layout stacked';
@content = @{get_ws_content($tmp)};
is(scalar @content, 1, 'Still one container on this ws');
is(scalar @{$content[0]->{nodes}}, 1, 'Stacked con has one child node');

cmd 'split h';
cmd 'open';
@content = @{get_ws_content($tmp)};
is(scalar @content, 1, 'Still one container on this ws');
is(scalar @{$content[0]->{nodes}}, 1, 'Stacked con still has one child node');

################################################################################
# When focusing the workspace, changing the layout should have an effect on the
# workspace, not on the parent (CT_CONTENT) container.
################################################################################

sub get_output_content {
    my $tree = i3(get_socket_path())->get_tree->recv;

    my @outputs = grep { $_->{name} !~ /^__/ } @{$tree->{nodes}};
    is(scalar @outputs, 1, 'exactly one output (testcase not multi-monitor capable)');
    my $output = $outputs[0];
    # get the first (and only) CT_CON
    return first { $_->{type} eq 'con' } @{$output->{nodes}};
}

$tmp = fresh_workspace;

cmd 'open';
cmd 'split v';
cmd 'open';
cmd 'focus parent';
is(get_output_content()->{layout}, 'splith', 'content container layout ok');
cmd 'layout stacked';
is(get_output_content()->{layout}, 'splith', 'content container layout still ok');

######################################################################
# Splitting a workspace that has more than one child
######################################################################

$tmp = fresh_workspace;

cmd 'open';
cmd 'open';
cmd 'focus parent';
cmd 'split v';
cmd 'open';

my $content = get_ws_content($tmp);
my $fst = $content->[0];
my $snd = $content->[1];

is(@{$content}, 2, 'two containers on workspace');
is(@{$fst->{nodes}}, 2, 'first child has two children');
is(@{$snd->{nodes}}, 0, 'second child has no children');

done_testing;
