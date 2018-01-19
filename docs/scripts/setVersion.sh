#!/bin/bash

if [[ -z "${TRAVIS_TAG}" ]]; then
	echo "Deploy snapshot version..."
	export RELEASE_VERSION="nightly"
else
	echo "Deploy release version: ${TRAVIS_TAG}..."
	export RELEASE_VERSION="${TRAVIS_TAG}"
fi
