#!/bin/sh

set -e

# .dockerignore is created on demand so that release.sh and other scripts are
# not influenced by our travis setup.
echo .git > .dockerignore

docker build --pull --no-cache --rm -t=${BASENAME} -f travis-build.Dockerfile .
docker login -e ${DOCKER_EMAIL} -u ${DOCKER_USER} -p ${DOCKER_PASS}
docker push ${BASENAME}
