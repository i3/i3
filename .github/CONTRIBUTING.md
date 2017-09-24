# Contributing

## i3status/i3lock bug reports and feature requests

Note that bug reports and feature requests for related projects should be filed in the corresponding repositories for [i3status](https://github.com/i3/i3status) and [i3lock](https://github.com/i3/i3lock).

## i3 bug reports and feature requests

1. Read the [debugging instructions](https://i3wm.org/docs/debugging.html).
2. Make sure you include a link to your logfile in your report (section 3).
3. Make sure you include the i3 version number in your report (section 1).
4. Please be aware that we cannot support compatibility issues with
   closed-source software, as digging into compatibility problems without
   having access to the source code is too time-consuming. Additionally,
   experience has shown that often, the software in question is responsible for
   the issue. Please raise an issue with the software in question, not i3.
5. Please note that i3 does not support compositors (e.g. compton). If you
   encountered the issue you are about to report while using a compositor,
   please try reproducing it without a compositor.

## Pull requests

* Before sending a pull request for new features, please check with us that the
  feature is something we want to see in i3 by opening an issue which has
  ”feature request” or ”enhancement” in its title.
* Use the `next` branch for developing and sending your pull request.
* Use `clang-format` to format your code.
* Run the [testsuite](https://i3wm.org/docs/testsuite.html)

## Finding something to do

* Find a [reproducible bug](https://github.com/i3/i3/issues?utf8=%E2%9C%93&q=is%3Aopen+label%3Areproducible+label%3Abug+) from the issue tracker. These issues have been reviewed and confirmed by a project contributor.
* Find an [accepted enhancement](https://github.com/i3/i3/issues?utf8=%E2%9C%93&q=is%3Aopen+label%3Aaccepted+label%3Aenhancement) from the issue tracker. These have been approved and are ok to start working on.

There's a very good [overview of the codebase](https://i3wm.org/docs/hacking-howto.html) available to get you started.
