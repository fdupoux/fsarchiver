#!/usr/bin/env bash

if [ -z ${DOCKER_CMD+x} ]; then
    # $DOCKER_CMD is not set -> use the default
    DOCKER_CMD=docker
fi

# Determine the path to the git repository
fullpath="$(realpath $0)"
curdir="$(dirname ${fullpath})"
repodir="$(realpath ${curdir}/..)"
echo "fullpath=${fullpath}"
echo "repodir=${repodir}"

# Build the docker image
dockerimg="fsarchiver-builder:latest"
"$DOCKER_CMD" build -t ${dockerimg} -f ${repodir}/docker/Dockerfile-archlinux ${repodir}/docker
