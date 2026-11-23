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
# counts only in lower case and as a whole word, as written. While the winners
# are loaded, displaced files and disabled names stay skipped below the line,
# and for the pending winners of enclosing lines. Variables in its patterns are
# filled in when the line is reached.
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

################################################################################
# While the files selected by a line are loaded, displaced files and disabled
# names are not loaded again below it, and a nested line disabling a name
# skips the pending files of that name of enclosing lines.
################################################################################

my $ssys = "$dir/scope-sys";
my $suser = "$dir/scope-user";
my $nested = "$dir/nested";
my $masks = "$dir/masks";
mkdir $ssys;
mkdir $suser;
mkdir $nested;
mkdir $masks;

write_file("$ssys/20-b", "this_is_not_a_directive\n");
write_file("$suser/20-b", "set \$unused b\n");
write_file("$ssys/40-null", "this_is_not_a_directive\n");
symlink('/dev/null', "$suser/40-null");
symlink("$ssys/20-b", "$nested/alias");
write_file("$nested/40-null", "set \$unused nested\n");
write_file("$suser/10-entry", "include $nested/alias\ninclude $nested/40-null\n");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $ssys/* $suser/*
EOT

is_deeply(included_files($config),
    [ "$suser/10-entry", "$suser/20-b" ],
    'displaced files and disabled names are not loaded below the line');

($ret, $out) = check_config($config);
is($ret, 0, 'displaced files are not read below the line');
is($out, '', 'no errors for files skipped below the line');

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $ssys/* $suser/*
include $nested/40-null
EOT

is_deeply(included_files($config),
    [ "$suser/10-entry", "$suser/20-b", "$nested/40-null" ],
    'decisions of a line end with the line');

write_file("$nested/05-entry", "include --override $masks/*\n");
write_file("$nested/50-slot", "set \$unused slot\n");
write_file("$nested/60-tail", "set \$unused tail\n");
symlink('/dev/null', "$masks/50-slot");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $nested/05-entry $nested/50-slot $nested/60-tail
EOT

is_deeply(included_files($config),
    [ "$nested/05-entry", "$nested/60-tail" ],
    'a nested line disables pending files of enclosing lines');

write_file("$nested/07-plain", "include $nested/05-entry $nested/50-slot\n");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $nested/07-plain
EOT

is_deeply(included_files($config),
    [ "$nested/07-plain", "$nested/05-entry", "$nested/50-slot" ],
    'files named by a plain include are loaded despite a nested disable');

################################################################################
# A file replaced on a nested line is skipped below that line however it is
# named (including by a hard link), and stays skipped for the pending files of
# enclosing lines, but only that file.
################################################################################

my $asys = "$dir/aside-sys";
my $auser = "$dir/aside-user";
my $aalt = "$dir/aside-alt";
my $aother = "$dir/aside-other";
mkdir $asys;
mkdir $auser;
mkdir $aalt;
mkdir $aother;

write_file("$asys/40-slot", "this_is_not_a_directive\n");
write_file("$aalt/40-slot", "set \$unused alt\n");
write_file("$aother/40-slot", "set \$unused other\n");
link("$asys/40-slot", "$aother/45-hardlink") or die "link: $!";
write_file("$auser/10-entry", "include --override $asys/40-slot $aalt/40-slot\n");
write_file("$auser/20-entry", "include $aother/45-hardlink\n");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $auser/10-entry $aother/45-hardlink
EOT

is_deeply(included_files($config),
    [ "$auser/10-entry", "$aalt/40-slot" ],
    'a file replaced on a nested line is skipped for enclosing lines');

($ret, $out) = check_config($config);
is($ret, 0, 'a file replaced on a nested line is not read afterwards');

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $auser/10-entry $aother/40-slot
EOT

is_deeply(included_files($config),
    [ "$auser/10-entry", "$aalt/40-slot", "$aother/40-slot" ],
    'a different file with the same name is still loaded');

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $asys/* $aother/40-slot $auser/20-entry
EOT

is_deeply(included_files($config),
    [ "$auser/20-entry", "$aother/40-slot" ],
    'a displaced file is skipped below the line when named by a hard link');

################################################################################
# Below the line, a looping link does not disable its name, unlike a dangling
# link.
################################################################################

my $lextra = "$dir/loop-extra";
mkdir $lextra;

symlink("$dir/does-not-exist", "$luser/70-dangling");
write_file("$lextra/60-loop", "set \$unused loop\n");
write_file("$lextra/70-dangling", "set \$unused dangling\n");
write_file("$luser/10-entry", "include $lextra/*\n");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $lsys/* $luser/*
EOT

is_deeply(included_files($config),
    [ "$luser/10-entry", "$lextra/60-loop" ],
    'a looping link does not disable its name, a dangling link does');

################################################################################
# Variables in the patterns of an include --override line are filled in when
# the line is reached: values set in files loaded before count, values set in
# displaced files do not, and a value set below a line only counts for later
# lines.
################################################################################

my $vsys = "$dir/vars-sys";
my $vuser = "$dir/vars-user";
my $vgood = "$dir/vars-good";
my $vbad = "$dir/vars-bad";
mkdir $vsys;
mkdir $vuser;
mkdir $vgood;
mkdir $vbad;

write_file("$vsys/10-paths", "set \$target $vbad\n");
write_file("$vuser/10-paths", "set \$target $vgood\n");
write_file("$vgood/10-redefine", "set \$target $vbad\n");
write_file("$vgood/20-good", "set \$unused good\n");
write_file("$vbad/20-bad", "set \$unused bad\n");

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $vsys/* $vuser/*
include --override \$target/*
EOT

is_deeply(included_files($config),
    [ "$vuser/10-paths", "$vgood/10-redefine", "$vgood/20-good" ],
    'variables set in files loaded before the line are filled in');

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

include --override $vsys/* $vuser/*
include --override \$target/*
include --override \$target/*
EOT

is_deeply(included_files($config),
    [ "$vuser/10-paths", "$vgood/10-redefine", "$vgood/20-good", "$vbad/20-bad" ],
    'a value set below a line counts for later lines');

################################################################################
# The variable-replaced contents in the GET_CONFIG reply show each
# include --override line as it was filled in when it was reached.
################################################################################

my $pid = launch_with_config($config);
my $i3 = i3(get_socket_path(0));
my $config_reply = $i3->get_config()->recv;
exit_gracefully($pid);

my $replaced = $config_reply->{included_configs}->[0]->{variable_replaced_contents};
my @override_lines = grep { /^include --override / } split(/\n/, $replaced);
is_deeply(\@override_lines,
    [ "include --override $vsys/* $vuser/*",
      "include --override $vgood/*",
      "include --override $vbad/*" ],
    'override lines are shown as filled in when they were reached');
like($config_reply->{included_configs}->[0]->{raw_contents},
    qr/^include --override \$target\/\*$/m,
    'raw contents keep the override lines as written');

################################################################################
# A carriage return separates directives like a line feed, alone or followed
# by a line feed. Each include --override line is filled in when it is reached,
# and the variable-replaced contents keep the line terminators.
################################################################################

for my $sep ("\n", "\r", "\r\n") {
    my $name = join('', map { $_ eq "\r" ? 'cr' : 'lf' } split(//, $sep));
    my $first = "$dir/$name-first";
    my $later = "$dir/$name-later";
    my $unused = "$dir/$name-unused";
    mkdir $first;
    mkdir $later;
    mkdir $unused;
    write_file("$first/10-a", "set \$later $later\n");
    write_file("$later/20-b", "# later\n");
    write_file("$unused/20-b", "# unused\n");

    my $lines = join($sep,
        "include --override \$first/*",
        "include --override \$later/*",
        "include --override \$first/*") . $sep;
    $config = "font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1\n"
        . "set \$first $first\n"
        . "set \$later $unused\n"
        . $lines;

    ($ret, $out) = check_config($config);
    is($ret, 0, "override lines separated by $name: config check passes");
    is($out, '', "override lines separated by $name: no errors");

    is_deeply(included_files($config),
        [ "$first/10-a", "$later/20-b" ],
        "override lines separated by $name are filled in when reached");

    $pid = launch_with_config($config);
    $i3 = i3(get_socket_path(0));
    $config_reply = $i3->get_config()->recv;
    exit_gracefully($pid);

    my $file = $config_reply->{included_configs}->[0];
    is(index($file->{raw_contents}, $lines), length($file->{raw_contents}) - length($lines),
        "override lines separated by $name: raw contents unchanged");
    my $filled = join($sep,
        "include --override $first/*",
        "include --override $later/*",
        "include --override $first/*") . $sep;
    is(substr($file->{variable_replaced_contents}, -length($filled)), $filled,
        "override lines separated by $name: each line shown as filled in, terminators kept");
}

################################################################################
# The patterns are taken from the filled-in line exactly as from a plain
# include line: a quoted value names several patterns. A flag which comes from
# a variable is a pattern of a plain include.
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set \$p "$sys/20-b $user/20-b"
include --override \$p
EOT

is_deeply(included_files($config),
    [ "$user/20-b" ],
    'a quoted value gives several patterns');

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set \$flag --override
include \$flag $sys/20-b $user/20-b
EOT

($ret, $out) = check_config($config);
is($ret, 1, 'a flag from a variable is a pattern of a plain include');
like($out, qr/[Dd]uplicate keybinding/, 'a flag from a variable loads every file');

################################################################################
# Variables are still filled in everywhere else, including in a quoted string
# which spans a line that looks like an include --override line.
################################################################################

$config = <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1

set \$v FILLED
mode "first
include --override \$v
last" {
    bindsym x nop
}
EOT

$pid = launch_with_config($config);
$i3 = i3(get_socket_path(0));
$i3->connect->recv;
my $modes = $i3->message(8, "")->recv;
exit_gracefully($pid);

ok((grep { $_ eq "first\ninclude --override FILLED\nlast" } @$modes),
    'variables are filled in within a quoted string spanning lines');

done_testing;
