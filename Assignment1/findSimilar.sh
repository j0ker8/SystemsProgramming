#!/bin/bash

OBJNAME=$1       #Stores the input filename
SEARCHDIR=" "    
if [ -z "$2" ]  #Handling the condition where directory name is not given
	then
	SEARCHDIR=$(pwd)  #Using present working directory if the -d arguement is not given
else
	SEARCHDIR="$2"
fi


read KEY_FILE_PATH FILE_USER FILE_HASH <<< $(bash sum_md5.sh $OBJNAME | tr ':' ' ')    #Using the sum_md5 script to store filepath, user and hashvalue of the input file

COUNTER=0   #Counter for counting similar files

while read -r line  #Handling line by line input from detect_similar script
do
	DIR=${line%->*}   #FULL absolute path of the matching file
	OBJ=${line#*->}   #filename for the matching file
	echo "-----DEBUG MESSAGE : Found Matching string at $DIR"

	if [ ! -d $FILE_HASH ]   #if the directory is not present
	then
		mkdir $FILE_HASH #make the direectory in the working directory
	fi

	MV_PATH=$(pwd)/$FILE_HASH/$OBJ	#Path to which the matching file will be moved
	if [[ ! -f $MV_PATH ]] #If the file is not in the folder
	then
    		echo "MOVING $DIR to $MV_PATH"
		mv $DIR $MV_PATH
	else                     #If the file is already present, append an index to the file name
		VER=1
		while [ -f $MV_PATH ]
		do
			CHARS=${OBJ%.*}
			EXT=${OBJ#*.}
			MV_PATH=$(pwd)/${FILE_HASH}/${CHARS}${VER}.$EXT
			((VER++))
		done
		echo "MOVING $DIR to $MV_PATH"
		mv $DIR $MV_PATH
	fi
	((COUNTER++))
	echo " "
	echo " "
	echo " "
done < <(bash sum_md5.sh $SEARCHDIR | bash detect_similar_inputformatted.sh $FILE_HASH)

echo "Number of Identical files to $OBJNAME : $COUNTER"
