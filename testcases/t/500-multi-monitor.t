#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests that the provided X-Server to the t/5??-*.t tests is actually providing
# multiple monitors.
#
use i3test;

my $i3 = i3(get_socket_path());

####################
# Request tree
####################

my $tree = $i3->get_tree->recv;

my @outputs = map { $_->{name} } @{$tree->{nodes}};
is_deeply(\@outputs, [ '__i3', 'xinerama-0', 'xinerama-1' ],
          'multi-monitor outputs ok');

done_testing;
