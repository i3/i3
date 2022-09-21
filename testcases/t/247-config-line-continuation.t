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
# • http://onyxneon.com/books/modern_perl/modern_perl_a4.pdf
#   (unless you are already familiar with Perl)
#
# Checks that the line continuation are parsed correctly
#

use i3test i3_autostart => 0;

# starts i3 with the given config, opens a window, returns its border style
sub launch_get_border {
    my ($config) = @_;

    my $pid = launch_with_config($config);

    my $i3 = i3(get_socket_path(0));
    my $tmp = fresh_workspace;

    my $window = open_window(name => '"special title"');

    my @content = @{get_ws_content($tmp)};
    cmp_ok(@content, '==', 1, 'one node on this workspace now');
    my $border = $content[0]->{border};

    exit_gracefully($pid);

    return $border;
}

#####################################################################
# test string escaping
#####################################################################

my $config = <<'EOT';
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set $vartest \"special title\"
for_window [title="$vartest"] border none
EOT

is(launch_get_border($config), 'none', 'no border');

#####################################################################
# test the line continuation
#####################################################################

$config = <<'EOT';
# i3 config file (v4)
font \
-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

# Use line continuation with too many lines (>4096 characters).
# This config is invalid. Use it to ensure no buffer overflow.
bindsym Mod1+b \
0001-This is a very very very very very very very very very very very very very very very very very long cmd \
0002-This is a very very very very very very very very very very very very very very very very very long cmd \
0003-This is a very very very very very very very very very very very very very very very very very long cmd \
0004-This is a very very very very very very very very very very very very very very very very very long cmd \
0005-This is a very very very very very very very very very very very very very very very very very long cmd \
0006-This is a very very very very very very very very very very very very very very very very very long cmd \
0007-This is a very very very very very very very very very very very very very very very very very long cmd \
0008-This is a very very very very very very very very very very very very very very very very very long cmd \
0009-This is a very very very very very very very very very very very very very very very very very long cmd \
0010-This is a very very very very very very very very very very very very very very very very very long cmd \
0011-This is a very very very very very very very very very very very very very very very very very long cmd \
0012-This is a very very very very very very very very very very very very very very very very very long cmd \
0013-This is a very very very very very very very very very very very very very very very very very long cmd \
0014-This is a very very very very very very very very very very very very very very very very very long cmd \
0015-This is a very very very very very very very very very very very very very very very very very long cmd \
0016-This is a very very very very very very very very very very very very very very very very very long cmd \
0017-This is a very very very very very very very very very very very very very very very very very long cmd \
0018-This is a very very very very very very very very very very very very very very very very very long cmd \
0019-This is a very very very very very very very very very very very very very very very very very long cmd \
0020-This is a very very very very very very very very very very very very very very very very very long cmd \
0021-This is a very very very very very very very very very very very very very very very very very long cmd \
0022-This is a very very very very very very very very very very very very very very very very very long cmd \
0023-This is a very very very very very very very very very very very very very very very very very long cmd \
0024-This is a very very very very very very very very very very very very very very very very very long cmd \
0025-This is a very very very very very very very very very very very very very very very very very long cmd \
0026-This is a very very very very very very very very very very very very very very very very very long cmd \
0027-This is a very very very very very very very very very very very very very very very very very long cmd \
0028-This is a very very very very very very very very very very very very very very very very very long cmd \
0029-This is a very very very very very very very very very very very very very very very very very long cmd \
0030-This is a very very very very very very very very very very very very very very very very very long cmd \
0031-This is a very very very very very very very very very very very very very very very very very long cmd \
0032-This is a very very very very very very very very very very very very very very very very very long cmd \
0033-This is a very very very very very very very very very very very very very very very very very long cmd \
0034-This is a very very very very very very very very very very very very very very very very very long cmd \
0035-This is a very very very very very very very very very very very very very very very very very long cmd \
0036-This is a very very very very very very very very very very very very very very very very very long cmd \
0037-This is a very very very very very very very very very very very very very very very very very long cmd \
0038-This is a very very very very very very very very very very very very very very very very very long cmd \
0039-This is a very very very very very very very very very very very very very very very very very long cmd \
0040-This is a very very very very very very very very very very very very very very very very very long cmd \
0041-This is a very very very very very very very very very very very very very very very very very long cmd \
0042-This is a very very very very very very very very very very very very very very very very very long cmd \
0043-This is a very very very very very very very very very very very very very very very very very long cmd \
0044-This is a very very very very very very very very very very very very very very very very very long cmd \
0045-This is a very very very very very very very very very very very very very very very very very long cmd \
0046-This is a very very very very very very very very very very very very very very very very very long cmd \
0047-This is a very very very very very very very very very very very very very very very very very long cmd \
0048-This is a very very very very very very very very very very very very very very very very very long cmd \
0049-This is a very very very very very very very very very very very very very very very very very long cmd \
0050-This is a very very very very very very very very very very very very very very very very very long cmd \
0051-This is a very very very very very very very very very very very very very very very very very long cmd \
0052-This is a very very very very very very very very very very very very very very very very very long cmd \
0053-This is a very very very very very very very very very very very very very very very very very long cmd \
0054-This is a very very very very very very very very very very very very very very very very very long cmd \
0055-This is a very very very very very very very very very very very very very very very very very long cmd \
0056-This is a very very very very very very very very very very very very very very very very very long cmd \
0057-This is a very very very very very very very very very very very very very very very very very long cmd \
0058-This is a very very very very very very very very very very very very very very very very very long cmd \
0059-This is a very very very very very very very very very very very very very very very very very long cmd \
0060-This is a very very very very very very very very very very very very very very very very very long cmd \
0061-This is a very very very very very very very very very very very very very very very very very long cmd \
0062-This is a very very very very very very very very very very very very very very very very very long cmd \
0063-This is a very very very very very very very very very very very very very very very very very long cmd \
0064-This is a very very very very very very very very very very very very very very very very very long cmd \
0065-This is a very very very very very very very very very very very very very very very very very long cmd \
0066-This is a very very very very very very very very very very very very very very very very very long cmd \
0067-This is a very very very very very very very very very very very very very very very very very long cmd \
0068-This is a very very very very very very very very very very very very very very very very very long cmd \
0069-This is a very very very very very very very very very very very very very very very very very long cmd \
0070-This is a very very very very very very very very very very very very very very very very very long cmd \
0071-This is a very very very very very very very very very very very very very very very very very long cmd \
0072-This is a very very very very very very very very very very very very very very very very very long cmd \
0073-This is a very very very very very very very very very very very very very very very very very long cmd \
0074-This is a very very very very very very very very very very very very very very very very very long cmd \
0075-This is a very very very very very very very very very very very very very very very very very long cmd \
0076-This is a very very very very very very very very very very very very very very very very very long cmd \
0077-This is a very very very very very very very very very very very very very very very very very long cmd \
0078-This is a very very very very very very very very very very very very very very very very very long cmd \
0079-This is a very very very very very very very very very very very very very very very very very long cmd \
0080-This is a very very very very very very very very very very very very very very very very very long cmd \
0081-This is a very very very very very very very very very very very very very very very very very long cmd \
0082-This is a very very very very very very very very very very very very very very very very very long cmd \
0083-This is a very very very very very very very very very very very very very very very very very long cmd \
0084-This is a very very very very very very very very very very very very very very very very very long cmd \
0085-This is a very very very very very very very very very very very very very very very very very long cmd \
0086-This is a very very very very very very very very very very very very very very very very very long cmd \
0087-This is a very very very very very very very very very very very very very very very very very long cmd \
0088-This is a very very very very very very very very very very very very very very very very very long cmd \
0089-This is a very very very very very very very very very very very very very very very very very long cmd \
0090-This is a very very very very very very very very very very very very very very very very very long cmd \
0091-This is a very very very very very very very very very very very very very very very very very long cmd \
0092-This is a very very very very very very very very very very very very very very very very very long cmd \
0093-This is a very very very very very very very very very very very very very very very very very long cmd \
0094-This is a very very very very very very very very very very very very very very very very very long cmd \
0095-This is a very very very very very very very very very very very very very very very very very long cmd \
0096-This is a very very very very very very very very very very very very very very very very very long cmd \
0097-This is a very very very very very very very very very very very very very very very very very long cmd \
0098-This is a very very very very very very very very very very very very very very very very very long cmd \
0099-This is a very very very very very very very very very very very very very very very very very long cmd

# Use line continuation for variables
set \
$vartest \
\"special title\"

# Use line continuation for commands
for_window \
[title="$vartest"] \
border \
none

EOT

is(launch_get_border($config), 'none', 'no border');

#####################################################################
# test the line continuation within a string
#####################################################################

$config = <<'EOT';
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set \
$vartest \
\"special \
title\"
for_window [title="$vartest"] border none
EOT

is(launch_get_border($config), 'none', 'no border');


#####################################################################
# test ignoring of line continuation within a comment
#####################################################################

$config = <<'EOT';
# i3 config file (v4)
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set $vartest \"special title\"
for_window [title="$vartest"] border pixel 1
# this line is not continued, so the following is not contained in this comment\
for_window [title="$vartest"] border none
EOT

is(launch_get_border($config), 'none', 'no border');

done_testing;
