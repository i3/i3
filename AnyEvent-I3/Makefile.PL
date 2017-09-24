use strict;
use warnings;

use 5.006;
use ExtUtils::MakeMaker;

if ( $^O eq 'MSWin32' ) {
    die "AnyEvent::I3 cannot be used on win32 (unix sockets are missing)";
}

my %meta = (
    name        => 'AnyEvent-I3',
    author      => 'Michael Stapelberg, C<< <michael at i3wm.org> >>',
    license     => ['perl'],
    'meta-spec' => { version => 2 },
    resources   => {
        repository => {
            url  => 'git://github.com/i3/i3',
            web  => 'https://github.com/i3/i3',
            type => 'git',
        },
        bugtracker => {
            web => 'https://github.com/i3/i3/issues',
        },
        homepage => 'https://i3wm.org/',
        license  => ['https://dev.perl.org/licenses'],
    },
);

my %requirements = (
    configure_requires => {
        'ExtUtils::MakeMaker' => 6.36,
    },
    build_requires => {
        'ExtUtils::MakeMaker' => 6.36
    },
    runtime_requires => {
        'AnyEvent'         => 0,
        'AnyEvent::Handle' => 0,
        'AnyEvent::Socket' => 0,
        'JSON::XS'         => 0,
    },
    test_requires => {
        'Test::More' => 0.80,
    },
);

my %merged_requirements = (
    'ExtUtils::MakeMaker' => 0,
    'AnyEvent'            => 0,
    'AnyEvent::Handle'    => 0,
    'AnyEvent::Socket'    => 0,
    'JSON::XS'            => 0,
    'Test::More'          => 0.80,
);

$meta{prereqs}{configure}{requires} = $requirements{configure_requires};
$meta{prereqs}{build}{requires}     = $requirements{build_requires};
$meta{prereqs}{runtime}{requires}   = $requirements{runtime_requires};
$meta{prereqs}{test}{requires}      = $requirements{test_requires};

my %MM_Args = (
    AUTHOR           => 'Michael Stapelberg',
    NAME             => 'AnyEvent::I3',
    DISTNAME         => 'AnyEvent-I3',
    EXE_FILES        => [],
    MIN_PERL_VERSION => '5.006',
    VERSION_FROM     => 'lib/AnyEvent/I3.pm',
    ABSTRACT_FROM    => 'lib/AnyEvent/I3.pm',
    test             => {
        TESTS => 't/*.t',
    },
);

sub is_eumm {
    eval { ExtUtils::MakeMaker->VERSION( $_[0] ) };
}

is_eumm(6.30) and $MM_Args{LICENSE} = $meta{license}[0];
is_eumm(6.47_01) or delete $MM_Args{MIN_PERL_VERSION};
is_eumm(6.52)
  and $MM_Args{CONFIGURE_REQUIRES} = $requirements{configure_requires};

is_eumm(6.57_02) and !is_eumm(6.57_07) and $MM_Args{NO_MYMETA} = 1;

if ( is_eumm(6.63_03) ) {
    %MM_Args = (
        %MM_Args,
        TEST_REQUIRES  => $requirements{test_requires},
        BUILD_REQUIRES => $requirements{build_requires},
        PREREQ_PM      => $requirements{runtime_requires},
    );
}
else {
    $MM_Args{PREREQ_PM} = \%merged_requirements;
}
unless ( -f 'META.yml' ) {
    $MM_Args{META_ADD} = \%meta;
}
WriteMakefile(%MM_Args);
