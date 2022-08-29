#!/bin/bash
if [ -z "$1" ]
then
	echo "Please enter a valid input"
else
	for i in $(find $1 -type f);
	do
		PATHVAR="$( realpath $i )"
		#echo "$(stat -c '%U' $i)"
		echo "$PATHVAR:$(stat -c '%U' $i):$(md5sum $PATHVAR | awk '{ print $1 }')"
	done
fi
