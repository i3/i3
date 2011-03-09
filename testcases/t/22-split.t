#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests splitting
#
use i3test;
use X11::XCB qw(:all);

my $tmp = fresh_workspace;

my $ws = get_ws($tmp);
is($ws->{orientation}, 'horizontal', 'orientation horizontal by default');
cmd 'split v';
$ws = get_ws($tmp);
is($ws->{orientation}, 'vertical', 'split v changes workspace orientation');

######################################################################
# Open two containers, split, open another container. Then verify
# the layout is like we expect it to be
######################################################################
cmd 'open';
cmd 'open';
my $content = get_ws_content($tmp);

is(@{$content}, 2, 'two containers on workspace level');
my $first = $content->[0];
my $second = $content->[1];

is(@{$first->{nodes}}, 0, 'first container has no children');
is(@{$second->{nodes}}, 0, 'second container has no children (yet)');
my $old_name = $second->{name};


cmd 'split h';
cmd 'open';

$content = get_ws_content($tmp);

is(@{$content}, 2, 'two containers on workspace level');
$first = $content->[0];
$second = $content->[1];

is(@{$first->{nodes}}, 0, 'first container has no children');
isnt($second->{name}, $old_name, 'second container was replaced');
is($second->{orientation}, 'horizontal', 'orientation is horizontal');
is(@{$second->{nodes}}, 2, 'second container has 2 children');
is($second->{nodes}->[0]->{name}, $old_name, 'found old second container');

# TODO: extend this test-case (test next/prev)
# - wrapping (no horizontal switch possible, goes level-up)
# - going level-up "manually"

######################################################################
# Test splitting multiple times without actually creating windows
######################################################################

$tmp = fresh_workspace;

$ws = get_ws($tmp);
is($ws->{orientation}, 'horizontal', 'orientation horizontal by default');
cmd 'split v';
$ws = get_ws($tmp);
is($ws->{orientation}, 'vertical', 'split v changes workspace orientation');

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

done_testing;
