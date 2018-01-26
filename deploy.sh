#!/bin/bash


if test "$CI_COMMIT_REF_NAME" = "master"; then
	#Nightly release
	version="nightly"
	prefix="nightly-`date +"%Y%m%d-%H%M%S"`/"
fi


if test -z "$BINTRAY_PROJECT" || test -z "$BINTRAY_KEY"; then
	echo "Project variables not set, skipping deployment"
	exit
fi

if test -z "$version"; then
	echo "Cannot deduce suitable version, skipping deployment"
fi

upload()
{
	localfile="$1"
	package="$2"
	remotefile="$3"

	curl -T "$localfile" -u"$BINTRAY_KEY" \
		"$BINTRAY_PROJECT/$package/$version/${prefix}${remotefile}?publish=1"
}

echo "Deploying to $BINTRAY_PROJECT, version $version"
if test "$CI_JOB_NAME" = "mingw-w64"; then
	upload "src/dispatch-ng.exe" "mingw-w64" "dispatch-ng.exe"
elif test "$CI_JOB_NAME" = "linux"; then
	filename="`ls | grep 'dispatch_ng.*\.tar\.gz'`"
	upload "$filename" "source" "dispatch_ng.tar.gz"
else
	echo "No deployment from $CI_JOB_NAME"
fi
echo
echo
