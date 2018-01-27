#!/bin/bash


if test "$CI_COMMIT_REF_NAME" = "master"; then
	#Nightly release
	version="nightly"
	prefix="nightly-`date +"%Y%m%d-%H%M%S"`/"
elif test "${CI_COMMIT_REF_NAME:0:8}" = "release-"; then
	version="${CI_COMMIT_REF_NAME:8}"
	prefix="release-$version"
fi


if test -z "$BINTRAY_PROJECT" || test -z "$BINTRAY_KEY"; then
	echo "Project variables not set, skipping deployment"
	exit
fi

if test -z "$version"; then
	echo "Cannot deduce suitable version, skipping deployment"
	exit
fi

upload()
{
	localfile="$1"
	package="$2"
	remotefile="$3"

	if test "$version" != "nightly"; then
		data="{ \"name\" : \"$version\""
		data="$data , \"desc\" : \"Stable release $version\" }"  
		curl -X POST -d "$data" -u"$BINTRAY_KEY" \
			"$BINTRAY_PROJECT/$package/versions"
	fi
	
	curl -T "$localfile" -u"$BINTRAY_KEY" \
		"$BINTRAY_PROJECT/$package/$version/${prefix}${remotefile}?publish=1"
	curl -X PUT -u"$BINTRAY_KEY" \
		"${BINTRAY_PROJECT/content/file_metadata}/${prefix}${remotefile}" \
		-d "{ \"list_in_downloads\":true }"
}

#curl()
#{
#	echo curl "$@"
#}

echo "Deploying to $BINTRAY_PROJECT, version $version, path $prefix"
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
