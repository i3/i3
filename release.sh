#!/bin/zsh
# This script is used to prepare a new release of i3.

set -eu

export RELEASE_VERSION="4.21"
export PREVIOUS_VERSION="4.20.1"
export RELEASE_BRANCH="next"

if [ ! -e "../i3.github.io" ]
then
	echo "../i3.github.io does not exist."
	echo "Use git clone https://github.com/i3/i3.github.io"
	exit 1
fi

if ! (cd ../i3.github.io && git pull)
then
	echo "Could not update ../i3.github.io repository"
	exit 1
fi

if git diff-files --quiet --exit-code debian/changelog
then
	echo "Expected debian/changelog to be changed (containing the changelog for ${RELEASE_VERSION})."
	exit 1
fi

if [ ! -e "RELEASE-NOTES-${RELEASE_VERSION}" ]
then
	echo "RELEASE-NOTES-${RELEASE_VERSION} not found. Here is the output from the generator:"
	./release-notes/generator.pl --print-urls
	exit 1
fi

eval $(gpg-agent --daemon)
export GPG_AGENT_INFO

################################################################################
# Section 1: update git and build the release tarball
################################################################################

STARTDIR=$PWD

TMPDIR=$(mktemp -d)
cd $TMPDIR
if ! wget https://i3wm.org/downloads/i3-${PREVIOUS_VERSION}.tar.xz; then
	echo "Could not download i3-${PREVIOUS_VERSION}.tar.xz (required for comparing files)."
	exit 1
fi
git clone --quiet --branch "${RELEASE_BRANCH}" https://github.com/i3/i3
cd i3
if [ ! -e "${STARTDIR}/RELEASE-NOTES-${RELEASE_VERSION}" ]; then
	echo "Required file RELEASE-NOTES-${RELEASE_VERSION} not found."
	exit 1
fi
git checkout -b release-${RELEASE_VERSION}
git rm RELEASE-NOTES-*
cp "${STARTDIR}/RELEASE-NOTES-${RELEASE_VERSION}" "RELEASE-NOTES-${RELEASE_VERSION}"
git add RELEASE-NOTES-${RELEASE_VERSION}
# Update the release version:
sed -i "s/^\s*version: '4.[^']*'/  version: '${RELEASE_VERSION}'/" meson.build
cp meson.build "${TMPDIR}/meson.build"
# Inject the release date into meson.build for the dist tarball:
sed -i "s/'-non-git'/' ($(date +'%Y-%m-%d'))'/" meson.build
git commit -a -m "release i3 ${RELEASE_VERSION}"
git tag "${RELEASE_VERSION}" -m "release i3 ${RELEASE_VERSION}" --sign --local-user=0x4AC8EE1D

mkdir build
(cd build && meson .. && ninja dist)
cp build/meson-dist/i3-${RELEASE_VERSION}.tar.xz .

echo "Differences in the release tarball file lists:"

diff --color -u \
	<(tar tf ../i3-${PREVIOUS_VERSION}.tar.* | sed "s,i3-${PREVIOUS_VERSION}/,,g" | sort) \
	<(tar tf    i3-${RELEASE_VERSION}.tar.xz  | sed "s,i3-${RELEASE_VERSION}/,,g"  | sort) || true

gpg --armor -b i3-${RELEASE_VERSION}.tar.xz

mv "${TMPDIR}/meson.build" .
git add meson.build
git commit -a -m "Restore non-git version suffix"

if [ "${RELEASE_BRANCH}" = "stable" ]; then
	git checkout stable
	git merge --no-ff release-${RELEASE_VERSION} -m "Merge branch 'release-${RELEASE_VERSION}'"
	git checkout next
	git merge --no-ff -s recursive -X ours -X no-renames stable -m "Merge branch 'stable' into next"
else
	git checkout next
	git merge --no-ff release-${RELEASE_VERSION} -m "Merge branch 'release-${RELEASE_VERSION}'"
	git checkout stable
	git merge --no-ff -s recursive -X theirs -X no-renames next -m "Merge branch 'next' into stable"
fi

git remote remove origin
git remote add origin git@github.com:i3/i3.git
git config --add remote.origin.push "+refs/tags/*:refs/tags/*"
git config --add remote.origin.push "+refs/heads/next:refs/heads/next"
git config --add remote.origin.push "+refs/heads/stable:refs/heads/stable"

################################################################################
# Section 2: Debian packaging (for QA)
################################################################################

cd "${TMPDIR}"
mkdir debian

# Copy over the changelog because we expect it to be locally modified in the
# start directory.
cp "${STARTDIR}/debian/changelog" i3/debian/changelog
(cd i3 && git add debian/changelog && git commit -m 'Update debian/changelog')

cat > ${TMPDIR}/Dockerfile <<EOT
FROM debian:sid
RUN echo deb-src http://deb.debian.org/debian sid main > /etc/apt/sources.list
RUN apt-get update && apt-get install -y dpkg-dev devscripts
COPY i3/i3-${RELEASE_VERSION}.tar.xz /usr/src/i3-wm_${RELEASE_VERSION}.orig.tar.xz
WORKDIR /usr/src/
RUN tar xf i3-wm_${RELEASE_VERSION}.orig.tar.xz
WORKDIR /usr/src/i3-${RELEASE_VERSION}
COPY i3/debian /usr/src/i3-${RELEASE_VERSION}/debian/
RUN mkdir debian/source
RUN echo '3.0 (quilt)' > debian/source/format
WORKDIR /usr/src
RUN mk-build-deps --install --remove --tool 'apt-get --no-install-recommends -y' i3-${RELEASE_VERSION}/debian/control
WORKDIR /usr/src/i3-${RELEASE_VERSION}
RUN dpkg-buildpackage -sa -j8
RUN dpkg-buildpackage -S -sa -j8
EOT

CONTAINER_NAME=$(echo "i3-${TMPDIR}" | sed 's,/,,g')
docker build --no-cache -t i3 .
for file in $(docker run --name "${CONTAINER_NAME}" i3 /bin/sh -c "ls /usr/src/i3*_${RELEASE_VERSION}*")
do
	docker cp "${CONTAINER_NAME}:${file}" ${TMPDIR}/debian/
done

echo "Content of resulting package’s .changes file:"
cat ${TMPDIR}/debian/*.changes

# TODO: docker cleanup

################################################################################
# Section 3: website
################################################################################

# Ensure we are in the correct branch for copying the docs.
cd ${TMPDIR}/i3
git checkout ${RELEASE_BRANCH}

cd ${TMPDIR}
git clone --quiet ${STARTDIR}/../i3.github.io
cd i3.github.io

mkdir docs/${PREVIOUS_VERSION}
tar cf - '--exclude=[0-9]\.[0-9e]*' docs | tar xf - --strip-components=1 -C docs/${PREVIOUS_VERSION}
git add docs/${PREVIOUS_VERSION}
git commit -a -m "save docs for ${PREVIOUS_VERSION}"

cp ${TMPDIR}/i3/i3-${RELEASE_VERSION}.tar.xz* downloads/
git add downloads/i3-${RELEASE_VERSION}.tar.xz*
cp ${TMPDIR}/i3/RELEASE-NOTES-${RELEASE_VERSION} downloads/RELEASE-NOTES-${RELEASE_VERSION}.txt
git add downloads/RELEASE-NOTES-${RELEASE_VERSION}.txt
sed -i "s,<h2>Documentation for i3 v[^<]*</h2>,<h2>Documentation for i3 v${RELEASE_VERSION}</h2>,g" docs/index.html
sed -i "s,\(span class=\"version\">\)[^<]*\(</span>\),\1${RELEASE_VERSION}\2,g" index.html
sed -i "s,The current stable version is .*$,The current stable version is ${RELEASE_VERSION}.,g" downloads/index.html
sed -i "s,<tbody>,<tbody>\n  <tr>\n    <td>${RELEASE_VERSION}</td>\n    <td><a href=\"/downloads/i3-${RELEASE_VERSION}.tar.xz\">i3-${RELEASE_VERSION}.tar.xz</a></td>\n    <td>$(LC_ALL=en_US.UTF-8 ls -lh ../i3/i3-${RELEASE_VERSION}.tar.xz | awk -F " " {'print $5'} | sed 's/K$/ KiB/g' | sed 's/M$/ MiB/g')</td>\n    <td><a href=\"/downloads/i3-${RELEASE_VERSION}.tar.xz.asc\">signature</a></td>\n    <td>$(date +'%Y-%m-%d')</td>\n    <td><a href=\"/downloads/RELEASE-NOTES-${RELEASE_VERSION}.txt\">release notes</a></td>\n  </tr>\n,g" downloads/index.html

git commit -a -m "add ${RELEASE_VERSION} release"

for i in $(find _docs -maxdepth 1 -and -type f -and \! -regex ".*\.\(html\|man\)$" -and \! -name "Makefile")
do
	base="$(basename $i)"
	[ -e "${TMPDIR}/i3/docs/${base}" ] && cp "${TMPDIR}/i3/docs/${base}" "_docs/${base}"
done

sed -i "s,Verify you are using i3 ≥ .*,Verify you are using i3 ≥ ${RELEASE_VERSION},g" _docs/debugging

(cd _docs && make)

for i in $(find _docs -maxdepth 1 -and -type f -and \! -regex ".*\.\(html\|man\|css\)$" -and \! -name "Makefile")
do
	base="$(basename $i)"
	[ -e "${TMPDIR}/i3/docs/${base}" ] && cp "_docs/${base}.html" docs/
done

git commit -a -m "update docs for ${RELEASE_VERSION}"

git remote remove origin
git remote add origin git@github.com:i3/i3.github.io.git
git config --add remote.origin.push "+refs/heads/main:refs/heads/main"

################################################################################
# Section 4: prepare release announcement email
################################################################################

cd ${TMPDIR}
cat >email.txt <<EOT
From: Michael Stapelberg <michael@i3wm.org>
To: i3-announce@freelists.org
Subject: i3 v${RELEASE_VERSION} released
Content-Type: text/plain; charset=utf-8
Content-Transfer-Encoding: 8bit

Hi,

I just released i3 v${RELEASE_VERSION}. Release notes follow:
EOT
cat ${TMPDIR}/i3/RELEASE-NOTES-${RELEASE_VERSION} >>email.txt

################################################################################
# Section 5: final push instructions
################################################################################

echo "As a final sanity check, install the debian package and see whether i3 works."

echo "When satisfied, run:"
echo "  cd ${TMPDIR}/i3"
echo "  git checkout next"
echo "  vi debian/changelog"
echo "  git commit -a -m \"debian: update changelog\""
echo "  git push"
echo ""
echo "  cd ${TMPDIR}/i3.github.io"
echo "  git push"
echo ""
echo "  cd ${TMPDIR}"
echo "  sendmail -t < email.txt"
echo ""
echo "Update milestones on GitHub (only for new major versions):"
echo "  Set due date of ${RELEASE_VERSION} to $(date +'%Y-%m-%d') and close the milestone"
echo "  Create milestone for the next major version with unset due date"
echo ""
echo "Announce on:"
echo "  twitter"
echo "  #i3 topic"
echo "  reddit /r/i3wm (link post to changelog)"
echo "  GitHub Discussions → Announcements"
