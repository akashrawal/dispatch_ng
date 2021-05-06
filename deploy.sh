#!/bin/bash


if test "$CI_COMMIT_REF_NAME" = "master"; then
	#Nightly release
	version="nightly"
	prefix="nightly-`date +"%Y%m%d-%H%M%S"`/"
elif test "${CI_COMMIT_REF_NAME:0:8}" = "release-"; then
	version="${CI_COMMIT_REF_NAME:8}"
	prefix="release-$version/"
fi

if test -z "$version"; then
	echo "Cannot deduce suitable version, skipping deployment"
	exit
fi

upload_root="$CI_API_V4_URL/projects/$CI_PROJECT_PATH/packages/generic"

curl_auth() {
	#Print out curl commandline but hide the job token"
	echo ">> curl $*" >&2
	curl --header "JOB-TOKEN: $CI_JOB_TOKEN" "$@"
	return $?
}

upload()
{
	localfile="$1"
	package="$2"
	remotefile="$3"

	curl_auth -T "$localfile"  \
		"$upload_root/$package/$version/$remotefile"
}


echo "Deploying to $upload_root, version $version, path $prefix"
if test "$CI_JOB_NAME" = "mingw-w64"; then
	upload "src/dispatch-ng.exe" "mingw-w64" "dispatch-ng.exe"
elif test "$CI_JOB_NAME" = "linux"; then
	filename="`ls | grep 'dispatch_ng.*\.tar\.gz'`"
	upload "$filename" "source" "$filename"
else
	echo "No deployment from $CI_JOB_NAME"
fi
echo
echo
