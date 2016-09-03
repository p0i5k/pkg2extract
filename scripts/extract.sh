#!/bin/bash

#input file
INPUT=upgrade_loader.pkg
#position of first record (logo), address of the second entry 'logo'
OFFSET=427

SIZE=`du -b "$INPUT" | cut -f1`

EXTRA_DATA_LEN=64

HEADER_LEN=48

echo "processing $INPUT size $SIZE"

while [ $OFFSET -lt $SIZE ]; do
	LEN_POS=8

	FNAME=`hexdump -s $OFFSET -n 4 -v -e '1/1 "%c"' $INPUT`
	FNAME="$FNAME.bin"

	#go to lenght
	OFFSET=$((OFFSET + LEN_POS))
	LEN=`hexdump -s $OFFSET  -n 4 -v -e '1/4 "%d"' $INPUT`
	LEN=$((LEN - EXTRA_DATA_LEN))
	#go to start of binary content
	OFFSET=$((OFFSET + 4 + HEADER_LEN + EXTRA_DATA_LEN))

	echo "saving $FNAME from offset $OFFSET, this may take a while..."

	dd skip=$OFFSET count=$LEN if=$INPUT of=$FNAME bs=1
	OFFSET=$((OFFSET + LEN))
done
