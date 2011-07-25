#!perl
# vim:ts=4:sw=4:expandtab

use i3test;
use List::MoreUtils qw(all none);
use List::Util qw(first);

my $i3 = i3(get_socket_path());

####################
# Request tree
####################

my $tree = $i3->get_tree->recv;

my $expected = {
    fullscreen_mode => 0,
    nodes => ignore(),
    window => undef,
    name => 'root',
    orientation => ignore(),
    type => 0,
    id => ignore(),
    rect => ignore(),
    window_rect => ignore(),
    geometry => ignore(),
    swallows => ignore(),
    percent => undef,
    layout => 'default',
    focus => ignore(),
    focused => JSON::XS::false,
    urgent => JSON::XS::false,
    border => 'normal',
    'floating_nodes' => ignore(),
};

cmp_deeply($tree, $expected, 'root node OK');

my @nodes = @{$tree->{nodes}};

ok(@nodes > 0, 'root node has at least one leaf');

ok((all { $_->{type} == 1 } @nodes), 'all nodes are of type CT_OUTPUT');
ok((none { defined($_->{window}) } @nodes), 'no CT_OUTPUT contains a window');
ok((all { @{$_->{nodes}} > 0 } @nodes), 'all nodes have at least one leaf (workspace)');
my @workspaces;
for my $ws (@nodes) {
    my $content = first { $_->{type} == 2 } @{$ws->{nodes}};
    @workspaces = (@workspaces, @{$content->{nodes}});
}

ok((all { $_->{type} == 4 } @workspaces), 'all workspaces are of type CT_WORKSPACE');
#ok((all { @{$_->{nodes}} == 0 } @workspaces), 'all workspaces are empty yet');
ok((none { defined($_->{window}) } @workspaces), 'no CT_OUTPUT contains a window');

# TODO: get the focused container

$i3->command('open')->recv;

# TODO: get the focused container, check if it changed.
# TODO: get the old focused container, check if there is a new child

#diag(Dumper(\@workspaces));

#diag(Dumper($tree));


done_testing;
