#!/bin/sh

set -e

BASENAME=$1
DOCKERFILE=$2

# .dockerignore is created on demand so that release.sh and other scripts are
# not influenced by our travis setup.
echo .git > .dockerignore

docker build --pull --no-cache --rm -t=${BASENAME} -f ${DOCKERFILE} .
# For pull requests, travis does not add secure environment variables to the
# environment (because pull requests could then steal their values), so skip
# the login+push step when the variable isn’t set.
if [ -n "${DOCKER_PASS}" ]
then
	docker login -u ${DOCKER_USER} -p ${DOCKER_PASS}
	docker push ${BASENAME}
fi
