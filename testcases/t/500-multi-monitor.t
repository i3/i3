#!perl
# vim:ts=4:sw=4:expandtab
#
# Tests that the provided X-Server to the t/5??-*.t tests is actually providing
# multiple monitors.
#
use i3test i3_autostart => 0;

my $config = <<EOT;
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT
my $pid = launch_with_config($config);

my $i3 = i3(get_socket_path());

####################
# Request tree
####################

my $tree = $i3->get_tree->recv;

my @outputs = map { $_->{name} } @{$tree->{nodes}};
is_deeply(\@outputs, [ '__i3', 'fake-0', 'fake-1' ],
          'multi-monitor outputs ok');

exit_gracefully($pid);

done_testing;
