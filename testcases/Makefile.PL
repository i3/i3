#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
use strict; use warnings;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME => 'i3-testsuite',
    MIN_PERL_VERSION => '5.010000', # 5.10.0
    PREREQ_PM => {
        'AnyEvent'     => 0,
        'AnyEvent::I3' => '0.16',
        'X11::XCB'     => '0.12',
        'Inline'       => 0,
        'Inline::C'    => 0,
        'ExtUtils::PkgConfig' => 0,
        'Test::More'   => '0.94',
        'IPC::Run' => 0,
    },
    PM => {}, # do not install any files from this directory
    clean => {
        FILES => 'testsuite-* latest i3-cfg-for-*',
    }
);

package MY;
sub test { } # do not run the tests while installing

# do not rename the Makefile
sub clean {
    my $section = shift->SUPER::clean(@_);
    $section =~ s/^\t\Q$_\E\n$//m for
        '- $(MV) $(FIRST_MAKEFILE) $(MAKEFILE_OLD) $(DEV_NULL)';
    $section;
}
