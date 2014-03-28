#!/bin/bash

count=0
date=`date`

#string=${date}

#for j in {1..12}
#do
#string=${string}${string}
#done

date

for i in {1..1000}
do
	name=$i #$RANDOM
	
	if [ ! -f ./a$name ]; then
		((count++))		
		echo $string >> ./a$name &
	fi
done

echo $count

date
