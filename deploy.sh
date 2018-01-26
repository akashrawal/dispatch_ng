#!/bin/bash


if test "$CI_COMMIT_REF_NAME" = "master"; then
	#Nightly release
	BINTRAY_VERSION="nightly"
fi


if test -z "$BINTRAY_PROJECT" || test -z "$BINTRAY_KEY"; then
	echo "Project variables not set, skipping deployment"
	exit
fi

if test -z "$BINTRAY_VERSION"; then
	echo "Cannot deduce suitable version, skipping deployment"
fi

echo "Deploying to $BINTRAY_PROJECT, version $BINTRAY_VERSION"
if test "$CI_JOB_NAME" = "mingw-w64"; then
	curl -T tests/dispatch-ng.exe -u$BINTRAY_KEY \
		$BINTRAY_PROJECT/mingw-w64/$BINTRAY_VERSION/dispatch-ng.exe
elif test "$CI_JOB_NAME" = "linux"; then
	filename="`ls | grep 'dispatch_ng.*\.tar\.gz'`"
	curl -T "$filename" -u$BINTRAY_KEY \
		$BINTRAY_PROJECT/source/$BINTRAY_VERSION/dispatch_ng.tar.gz
else
	echo "No deployment from $CI_JOB_NAME"
fi
