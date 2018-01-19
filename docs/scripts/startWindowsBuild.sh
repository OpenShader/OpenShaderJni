#!/bin/bash

#if [[ $(curl -H "Authorization: Bearer $APPVEYOR_TOKEN" -H "Content-Type: application/json" -X POST https://ci.appveyor.com/api/builds -D '{ "accountName": "szszss", "projectSlug": "openshaderjni", "branch": "master"' | grep 'status') != *queued* ]]; then
#  exit -1
#fi
echo "Waiting for jobs of AppVeyor..."
#sleep 120s
for ((i=0; i < 360; i++))
do
	sleep 10s
	result=$(curl -H "Authorization: Bearer $APPVEYOR_TOKEN" -H "Content-Type: application/json" -X GET https://ci.appveyor.com/api/projects/szszss/openshaderjni | JSON.sh)
	#echo "$result"
	if [[ "$result" == *'status"]	"fail'* ]]; then
		echo "Works failed."
		exit -2
	fi
	if [[ "$result" == *'["build","status"]	"success"'* ]]; then
		echo "Works completed."
		exit 0
	fi
	echo "Checking jobs of AppVeyor... Not finish yet."
done
echo "Out of time!"
exit -3
