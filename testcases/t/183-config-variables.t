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
# Checks that variables are parsed correctly by using for_window rules with
# variables in it.
#

use i3test i3_autostart => 0;

# starts i3 with the given config, opens a window, returns its border style
sub launch_get_border {
    my ($config) = @_;

    my $pid = launch_with_config($config);

    my $i3 = i3(get_socket_path(0));
    my $tmp = fresh_workspace;

    my $window = open_window(name => 'special title');

    my @content = @{get_ws_content($tmp)};
    cmp_ok(@content, '==', 1, 'one node on this workspace now');
    my $border = $content[0]->{border};

    exit_gracefully($pid);

    return $border;
}

#####################################################################
# test thet windows get the default border
#####################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
EOT

is(launch_get_border($config), 'normal', 'normal border');

#####################################################################
# now use a variable and for_window
#####################################################################

$config = <<'EOT';
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set $vartest special title
for_window [title="$vartest"] border none
EOT

is(launch_get_border($config), 'none', 'no border');

#####################################################################
# check that whitespaces and tabs are ignored
#####################################################################

$config = <<'EOT';
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set               $vartest             special title
for_window [title="$vartest"] border none
EOT

is(launch_get_border($config), 'none', 'no border');

#####################################################################
# test that longest matching variable name is substituted
#####################################################################

$config = <<'EOT';
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set $var normal title
set $vartest special title
set $vart mundane title
for_window [title="$vartest"] border none
EOT

is(launch_get_border($config), 'none', 'no border');

#####################################################################
# test that variables with longer name than value don't crash i3 with
# v3 to v4 conversion.
# See: #3076
#####################################################################

$config = <<'EOT';
set $var a
EOT

my $pid = launch_with_config($config);
does_i3_live;
exit_gracefully($pid);

done_testing;

