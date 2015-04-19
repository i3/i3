# i3status/i3lock bugreports/feature requests

Note that i3status and i3lock related bugreports and feature requests should be
filed in the corresponding repositories, i.e. https://github.com/i3/i3status
and https://github.com/i3/i3lock

# i3 bugreports/feature requests

1. Read http://i3wm.org/docs/debugging.html
2. Make sure you include a link to your logfile in your report (section 3).
3. Make sure you include the i3 version number in your report (section 1).
4. Please be aware that we cannot support compatibility issues with
   closed-source software, as digging into compatibility problems without
   having access to the source code is too time-consuming. Additionally,
   experience has shown that often, the software in question is responsible for
   the issue. Please raise an issue with the software in question, not i3.

# Pull requests

* Before sending a pull request for new features, please check with us that the
  feature is something we want to see in i3 by opening an issue which has
  “feature request” or “enhancement” in its title.
* Use the `next` branch for developing and sending your pull request.
* Use `clang-format` to format your code.
* Run the testsuite, see http://i3wm.org/docs/testsuite.html
