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
# Verifies the include --override directive: among the files matched by one
# line, the last file with a given name wins, empty files and links to
# /dev/null disable a name, and the winners are loaded in byte order. The flag
# counts only in lower case and as a whole word.
#
use File::Temp qw(tempfile tempdir);
use Cwd qw(abs_path);
use i3test i3_autostart => 0;

my $dir = abs_path(tempdir(CLEANUP => 1));
my $sys = "$dir/sys";
my $user = "$dir/user";
mkdir $sys;
mkdir $user;

sub write_file {
    my ($path, $contents) = @_;
    open(my $fh, '>', $path) or die "open $path: $!";
    print $fh $contents;
    close($fh);
}

sub included_files {
    my ($config) = @_;

    my $pid = launch_with_config($config);
    my $i3 = i3(get_socket_path(0));
    my $included = $i3->get_version()->recv->{included_config_file_names};
    exit_gracefully($pid);

    return $included;
}

sub check_config {
    my ($config) = @_;
    my ($fh, $tmpfile) = tempfile(UNLINK => 1);
    print $fh $config;
    $fh->flush;
    my $output = qx(DISPLAY= i3 -C -c $tmpfile 2>&1);
    my $retval = $?;
    close($fh);
    return ($retval >> 8, $output);
}

write_file("$sys/10-a", "set \$unused a\n");
# Both files bind the same key: since the shipped file is replaced, this must
# not result in a duplicate binding error. It also contains an error, which
# must not matter as the file is never read.
write_file("$sys/20-b", "bindsym Mod4+b nop shipped\nthis_is_not_a_directive\n");
write_file("$user/20-b", "bindsym Mod4+b nop user\n");
write_file("$user/05-c", "set \$unused c\n");
write_file("$sys/30-empty", "this_is_not_a_directive\n");
write_file("$user/30-empty", "");
write_file("$sys/40-null", "this_is_not_a_directive\n");
symlink('/dev/null', "$user/40-null");
write_file("$sys/50-dangling", "this_is_not_a_directive\n");
symlink("$dir/does-not-exist", "$user/50-dangling");

################################################################################
# The last file with a name wins, winners are loaded in byte order.
################################################################################

my $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $sys/* $user/* $dir/missing/*
EOT

is_deeply(included_files($config),
    [ "$user/05-c", "$sys/10-a", "$user/20-b", "$user/30-empty" ],
    'later files replace earlier files with the same name');

my ($ret, $out) = check_config($config);
is($ret, 0, 'replaced files are not read');
is($out, '', 'no errors for replaced files');

################################################################################
# Plain include still loads every file.
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include $sys/10-a $sys/20-b $user/20-b
EOT

($ret, $out) = check_config($config);
is($ret, 1, 'plain include loads all files');
like($out, qr/[Dd]uplicate keybinding/, 'plain include reports duplicate bindings');

################################################################################
# Competition is confined to a single line.
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $sys/10-a
include --override $user/05-c $sys/../user/../sys/10-a
EOT

is_deeply(included_files($config),
    [ "$sys/10-a", "$user/05-c" ],
    'separate lines do not replace each other, files are loaded once');

################################################################################
# Winners are loaded in byte order of their names, whatever the order of the
# patterns.
################################################################################

my $order = "$dir/order";
mkdir $order;
write_file("$order/$_", "set \$unused $_\n") for qw(3-x 20_z 20-alpha 20-Beta);

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $order/3-x $order/20_z $order/20-alpha $order/20-Beta
EOT

is_deeply(included_files($config),
    [ map { "$order/$_" } qw(20-Beta 20-alpha 20_z 3-x) ],
    'winners are loaded in byte order of their names');

################################################################################
# The flag without a pattern is an error.
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override
EOT

($ret, $out) = check_config($config);
is($ret, 1, 'include --override without a pattern is an error');
like($out, qr/ERROR: *CONFIG: *[Ee]xpected.*tokens/, 'missing pattern reported');

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override # $sys/* $user/*
EOT

($ret, $out) = check_config($config);
is($ret, 1, 'include --override followed by a comment is an error');
like($out, qr/ERROR: *CONFIG: *[Ee]xpected.*tokens/, 'comment after the flag reported');

################################################################################
# The flag counts only in lower case and as a whole word. Otherwise it is a
# pattern of a plain include, which loads every file. A repeated flag is a
# pattern, too.
################################################################################

for my $flag ('--OVERRIDE', '--Override', '--override.conf', '--override-backup') {
    $config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include $flag $sys/20-b $user/20-b
EOT

    ($ret, $out) = check_config($config);
    is($ret, 1, "include $flag is a plain include");
    like($out, qr/[Dd]uplicate keybinding/, "include $flag loads every file");
}

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override --override $sys/20-b $user/20-b
EOT

is_deeply(included_files($config),
    [ "$user/20-b" ],
    'a repeated flag is a pattern of the include --override line');

################################################################################
# A winning symbolic link which cannot be followed for another reason than a
# missing target (here: a loop) is skipped like a path of a plain include. It
# still replaces the earlier files of its name.
################################################################################

my $lsys = "$dir/loop-sys";
my $luser = "$dir/loop-user";
mkdir $lsys;
mkdir $luser;

write_file("$lsys/60-loop", "this_is_not_a_directive\n");
symlink("$luser/60-loop", "$luser/60-loop");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $lsys/60-loop $luser/60-loop
EOT

($ret, $out) = check_config($config);
is($ret, 0, 'a looping link replaces the file of its name');
is($out, '', 'no errors for a looping link');

done_testing;
