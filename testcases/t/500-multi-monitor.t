#!perl
# vim:ts=4:sw=4:expandtab
#
# Please read the following documents before working on tests:
# • https://build.i3wm.org/docs/testsuite.html
#   (or docs/testsuite)
#
# • https://build.i3wm.org/docs/lib-i3test.html
#   (alternatively: perldoc ./testcases/lib/i3test.pm)
#
# • https://build.i3wm.org/docs/ipc.html
#   (or docs/ipc)
#
# • https://i3wm.org/downloads/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Tests that the provided X-Server to the t/5??-*.t tests is actually providing
# multiple monitors.
#
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

fake-outputs 1024x768+0+0,1024x768+1024+0
EOT

my $i3 = i3(get_socket_path());

####################
# Request tree
####################

my $tree = $i3->get_tree->recv;

my @outputs = map { $_->{name} } @{$tree->{nodes}};
is_deeply(\@outputs, [ '__i3', 'fake-0', 'fake-1' ],
          'multi-monitor outputs ok');


done_testing;
