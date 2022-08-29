#!/bin/bash

DAHASH=$1
IFS=':'
CURRUSER=$(whoami)
COUNTERR=0
FIRSTENTRY=" "
FIRSTLINE=" "
while read FILEPATH USER HASHVAL 
do
	if [ "$HASHVAL" == "$DAHASH" ] && [ "$USER" == "$CURRUSER" ]
		then
		((COUNTERR++))
		NM="$(basename ${FILEPATH})"
		OUTPUT="${FILEPATH}->${NM}"
		if [ "$COUNTERR" == 1 ]
			then
			#FIRSTLINE="-  $DAHASH"
			FIRSTENTRY=$OUTPUT
		elif [ "$COUNTERR" == 2 ]
			then
			#echo "$FIRSTLINE"
			echo "$FIRSTENTRY"
			echo "$OUTPUT"
		else
			echo "$OUTPUT"
			#echo "LAST ELSE BLOCK"
		fi
	fi
done
