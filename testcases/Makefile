test:
	PERL_DL_NONLAZY=1 /usr/bin/perl "-MExtUtils::Command::MM" "-e" "test_harness(1)" -It/lib t/21*

all: test

testfull:
	PERL_DL_NONLAZY=1 /usr/bin/perl "-MExtUtils::Command::MM" "-e" "test_harness(1)" -It/lib t/01* t/02* t/03* t/05* t/17* t/18* t/19* t/20*
