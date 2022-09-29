#!/bin/bash -e

DOCKER_IMAGE='ubuntu:16.04'

docker pull "$DOCKER_IMAGE"
docker container run -t --rm -v "$(pwd)":/io --env INSTALL_DEPS=1 "$DOCKER_IMAGE" /io/build.sh
