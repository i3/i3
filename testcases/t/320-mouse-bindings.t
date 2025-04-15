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
# Test button bindsyms
use i3test i3_config => <<EOT;
font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
focus_follows_mouse no

for_window[class="mark_A"] mark A
for_window[class="mark_B"] mark B
for_window[class="mark_C"] mark C
for_window[class="mark_D"] mark D

bindsym button1 --whole-window [con_mark=A] focus
bindsym button2 --whole-window [con_mark=B] focus

bindsym button4 --whole-window [con_mark=B] focus
mode "testmode" {
    bindsym button4 --whole-window [con_mark=C] focus
    bindsym button5 --whole-window [con_mark=D] focus
}

default_border pixel 0

EOT
use i3test::XTEST;

sub button {
    my ($button, $window, $msg) = @_;
    xtest_button_press($button, 5, 5);
    xtest_button_release($button, 5, 5);
    xtest_sync_with_i3;

    local $Test::Builder::Level = $Test::Builder::Level + 1;
    is_focus($window, $msg);
}

sub is_focus {
    my ($window, $msg) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;
    is($x->input_focus, $window->id, $msg);
}

# Leftmost window is focused on button presses that have no binding
my $L = open_window;
my $A = open_window(wm_class => 'mark_A');
my $B = open_window(wm_class => 'mark_B');
is_focus($B, 'sanity check');
is_focus(open_window, 'sanity check, other window');

button(1, $A, 'button 1 binding');
button(1, $A, 'button 1 binding, again');
button(2, $B, 'button 2 binding');
button(1, $A, 'button 1 binding');
button(3, $L, 'button 3, no binding');

# Test modes, see #4539
# Unfortunately, grabbing / ungrabbing doesn't seem to work correctly in xvfb
# so we can't really test this.

my $C = open_window(wm_class => 'mark_C');
my $D = open_window(wm_class => 'mark_D');

button(4, $B, 'button 4 binding outside mode');
button(5, $L, 'button 5 no binding outside mode');

cmd 'mode testmode';
button(4, $C, 'button 4 binding inside mode');
button(5, $D, 'button 5 binding inside mode');

cmd 'mode default';
button(4, $B, 'button 4 binding outside mode');
button(5, $L, 'button 5 no binding outside mode');

done_testing;
