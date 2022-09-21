# vim:ft=Dockerfile
# Same as travis-base.Dockerfile, but without the test suite dependencies since
# we only build Debian packages on Ubuntu, we don’t run the tests.
FROM ubuntu:bionic

RUN echo force-unsafe-io > /etc/dpkg/dpkg.cfg.d/docker-apt-speedup
# Paper over occasional network flakiness of some mirrors.
RUN echo 'APT::Acquire::Retries "5";' > /etc/apt/apt.conf.d/80retry

# NOTE: I tried exclusively using gce_debian_mirror.storage.googleapis.com
# instead of httpredir.debian.org, but the results (Fetched 123 MB in 36s (3357
# kB/s)) are not any better than httpredir.debian.org (Fetched 123 MB in 34s
# (3608 kB/s)). Hence, let’s stick with httpredir.debian.org (default) for now.

# Install mk-build-deps (for installing the i3 build dependencies),
# lintian (for checking spelling errors),
# test suite dependencies (for running tests)
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    dpkg-dev devscripts git equivs \
    build-essential clang \
    lintian && \
    rm -rf /var/lib/apt/lists/*

# Install i3 build dependencies.
COPY debian/control /usr/src/i3-debian-packaging/control
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive mk-build-deps --install --remove --tool 'apt-get --no-install-recommends -y' /usr/src/i3-debian-packaging/control && \
    rm -rf /var/lib/apt/lists/*

# The user outside of Docker (GitHub Actions CI runner) and inside of Docker
# (root) are different, and newer versions of git error out in that scenario.
# To fix this, explicitly configure /usr/src/i3 as a safe directory:
RUN git config --global --add safe.directory /usr/src/i3
