#!/usr/bin/env perl
# vim:ts=4:sw=4:expandtab
use strict; use warnings;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME => 'i3 testsuite',
    MIN_PERL_VERSION => '5.010000', # 5.10.0
    PREREQ_PM => {
        'AnyEvent'     => 0,
        'AnyEvent::I3' => '0.09',
        'X11::XCB'     => '0.03',
        'Test::Most'   => 0,
        'Test::Deep'   => 0,
	'EV'	       => 0,
	'Inline'       => 0,
    },
    # don't install any files from this directory
    PM => {},
    clean => {
        FILES => 'testsuite-* latest'
    }
);
# and don't run the tests while installing
sub MY::test { }
