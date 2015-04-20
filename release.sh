#!/bin/zsh
# This script is used to prepare a new release of i3.

export RELEASE_VERSION="4.10.2"
export PREVIOUS_VERSION="4.10.1"
export RELEASE_BRANCH="master"

if [ ! -e "../i3.github.io" ]
then
	echo "../i3.github.io does not exist."
	echo "Use git clone git://github.com/i3/i3.github.io"
	exit 1
fi

if [ ! -e "RELEASE-NOTES-${RELEASE_VERSION}" ]
then
	echo "RELEASE-NOTES-${RELEASE_VERSION} not found."
	exit 1
fi

if git diff-files --quiet --exit-code debian/changelog
then
	echo "Expected debian/changelog to be changed (containing the changelog for ${RELEASE_VERSION})."
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
if ! wget http://i3wm.org/downloads/i3-${PREVIOUS_VERSION}.tar.bz2; then
	echo "Could not download i3-${PREVIOUS_VERSION}.tar.bz2 (required for comparing files)."
	exit 1
fi
git clone --quiet --branch "${RELEASE_BRANCH}" file://${STARTDIR}
cd i3
if [ ! -e "${STARTDIR}/RELEASE-NOTES-${RELEASE_VERSION}" ]; then
	echo "Required file RELEASE-NOTES-${RELEASE_VERSION} not found."
	exit 1
fi
git checkout -b release-${RELEASE_VERSION}
cp "${STARTDIR}/RELEASE-NOTES-${RELEASE_VERSION}" "RELEASE-NOTES-${RELEASE_VERSION}"
git add RELEASE-NOTES-${RELEASE_VERSION}
git rm RELEASE-NOTES-${PREVIOUS_VERSION}
sed -i "s,<refmiscinfo class=\"version\">[^<]*</refmiscinfo>,<refmiscinfo class=\"version\">${RELEASE_VERSION}</refmiscinfo>,g" man/asciidoc.conf
git commit -a -m "release i3 ${RELEASE_VERSION}"
git tag "${RELEASE_VERSION}" -m "release i3 ${RELEASE_VERSION}" --sign --local-user=0x4AC8EE1D

make dist

echo "Differences in the release tarball file lists:"

diff -u \
	<(tar tf ../i3-${PREVIOUS_VERSION}.tar.bz2 | sed "s,i3-${PREVIOUS_VERSION}/,,g" | sort) \
	<(tar tf    i3-${RELEASE_VERSION}.tar.bz2  | sed "s,i3-${RELEASE_VERSION}/,,g"  | sort) \
	| colordiff

if ! tar xf i3-${RELEASE_VERSION}.tar.bz2 --to-stdout --strip-components=1 i3-${RELEASE_VERSION}/I3_VERSION | grep -q "^${RELEASE_VERSION} "
then
	echo "I3_VERSION file does not start with ${RELEASE_VERSION}"
	exit 1
fi

gpg --armor -b i3-${RELEASE_VERSION}.tar.bz2

if [ "${RELEASE_BRANCH}" = "master" ]; then
	git checkout master
	git merge --no-ff release-${RELEASE_VERSION} -m "Merge branch 'release-${RELEASE_VERSION}'"
	git checkout next
	git merge --no-ff master -m "Merge branch 'master' into next"
else
	git checkout next
	git merge --no-ff release-${RELEASE_VERSION} -m "Merge branch 'release-${RELEASE_VERSION}'"
	git checkout master
	git merge --no-ff next -m "Merge branch 'next' into master"
fi

git remote remove origin
git remote add origin git@github.com:i3/i3.git
git config --add remote.origin.push "+refs/tags/*:refs/tags/*"
git config --add remote.origin.push "+refs/heads/next:refs/heads/next"
git config --add remote.origin.push "+refs/heads/master:refs/heads/master"

################################################################################
# Section 2: Debian packaging
################################################################################

cd "${TMPDIR}"
mkdir debian

# Copy over the changelog because we expect it to be locally modified in the
# start directory.
cp "${STARTDIR}/debian/changelog" i3/debian/changelog

cat > ${TMPDIR}/Dockerfile <<EOT
FROM debian:sid
RUN sed -i 's,^deb \(.*\),deb \1\ndeb-src \1,g' /etc/apt/sources.list
RUN apt-get update && apt-get install -y dpkg-dev devscripts
COPY i3/i3-${RELEASE_VERSION}.tar.bz2 /usr/src/i3-wm_${RELEASE_VERSION}.orig.tar.bz2
WORKDIR /usr/src/
RUN tar xf i3-wm_${RELEASE_VERSION}.orig.tar.bz2
WORKDIR /usr/src/i3-${RELEASE_VERSION}
COPY i3/debian /usr/src/i3-${RELEASE_VERSION}/debian/
RUN mkdir debian/source
RUN echo '3.0 (quilt)' > debian/source/format
WORKDIR /usr/src
RUN mk-build-deps --install --remove --tool 'apt-get --no-install-recommends -y' i3-${RELEASE_VERSION}/debian/control
WORKDIR /usr/src/i3-${RELEASE_VERSION}
RUN dpkg-buildpackage -sa -j8
EOT

CONTAINER_NAME=$(echo "i3-${TMPDIR}" | sed 's,/,,g')
docker build -t i3 .
for file in $(docker run --name "${CONTAINER_NAME}" i3 /bin/sh -c "ls /usr/src/i3*_${RELEASE_VERSION}*")
do
	docker cp "${CONTAINER_NAME}:${file}" ${TMPDIR}/debian/
done

echo "Content of resulting package’s .changes file:"
cat ${TMPDIR}/debian/*.changes

# debsign is in devscripts, which is available in fedora and debian
debsign -k4AC8EE1D ${TMPDIR}/debian/*.changes

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
cp ${TMPDIR}/i3/i3-${RELEASE_VERSION}.tar.bz2* downloads/
git add downloads/i3-${RELEASE_VERSION}.tar.bz2*
cp ${TMPDIR}/i3/RELEASE-NOTES-${RELEASE_VERSION} downloads/RELEASE-NOTES-${RELEASE_VERSION}.txt
git add downloads/RELEASE-NOTES-${RELEASE_VERSION}.txt
sed -i "s,<h2>Documentation for i3 v[^<]*</h2>,<h2>Documentation for i3 v${RELEASE_VERSION}</h2>,g" docs/index.html
sed -i "s,Verify you are using i3 ≥ .*,Verify you are using i3 ≥ ${RELEASE_VERSION},g" docs/debugging.html
sed -i "s,<span style=\"margin-left: 2em; color: #c0c0c0\">[^<]*</span>,<span style=\"margin-left: 2em; color: #c0c0c0\">${RELEASE_VERSION}</span>,g" index.html
sed -i "s,The current stable version is .*$,The current stable version is ${RELEASE_VERSION}.,g" downloads/index.html
sed -i "s,<tbody>,<tbody>\n  <tr>\n    <td>${RELEASE_VERSION}</td>\n    <td><a href=\"/downloads/i3-${RELEASE_VERSION}.tar.bz2\">i3-${RELEASE_VERSION}.tar.bz2</a></td>\n    <td>$(ls -lh ../i3/i3-${RELEASE_VERSION}.tar.bz2 | awk -F " " {'print $5'} | sed 's/K$/ KiB/g')</td>\n    <td><a href=\"/downloads/i3-${RELEASE_VERSION}.tar.bz2.asc\">signature</a></td>\n    <td>$(date +'%Y-%m-%d')</td>\n    <td><a href=\"/downloads/RELEASE-NOTES-${RELEASE_VERSION}.txt\">release notes</a></td>\n  </tr>\n,g" downloads/index.html

git commit -a -m "add ${RELEASE_VERSION} release"

mkdir docs/${PREVIOUS_VERSION}
tar cf - '--exclude=[0-9]\.[0-9e]*' docs | tar xf - --strip-components=1 -C docs/${PREVIOUS_VERSION}
git add docs/${PREVIOUS_VERSION}
git commit -a -m "save docs for ${PREVIOUS_VERSION}"

for i in $(find _docs -maxdepth 1 -and -type f -and \! -regex ".*\.\(html\|man\)$" -and \! -name "Makefile")
do
	base="$(basename $i)"
	[ -e "${STARTDIR}/docs/${base}" ] && cp "${STARTDIR}/docs/${base}" "_docs/${base}"
done

(cd _docs && make)

for i in $(find _docs -maxdepth 1 -and -type f -and \! -regex ".*\.\(html\|man\)$" -and \! -name "Makefile")
do
	base="$(basename $i)"
	[ -e "${STARTDIR}/docs/${base}" ] && cp "_docs/${base}.html" docs/
done

git commit -a -m "update docs for ${RELEASE_VERSION}"

git remote remove origin
git remote add origin git@github.com:i3/i3.github.io.git
git config --add remote.origin.push "+refs/heads/master:refs/heads/master"

################################################################################
# Section 4: prepare release announcement email
################################################################################

cd ${TMPDIR}
cat >email.txt <<EOT
From: Michael Stapelberg <michael@i3wm.org>
To: i3-announce@i3.zekjur.net
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
echo "  cd ${TMPDIR}/debian"
echo "  dput *.changes"
echo ""
echo "  cd ${TMPDIR}"
echo "  sendmail -t < email.txt"
echo ""
echo "Announce on:"
echo "  twitter"
echo "  google+"
echo "  mailing list"
echo "  #i3 topic"
