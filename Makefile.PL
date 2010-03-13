use inc::Module::Install;

name     'AnyEvent-I3';
all_from 'lib/AnyEvent/I3.pm';
author   'Michael Stapelberg';

requires 'AnyEvent';
requires 'AnyEvent::Handle';
requires 'AnyEvent::Socket';
requires 'JSON::XS';

WriteAll;
