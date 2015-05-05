#!/bin/sh

set -e

TEMPFILE=$(mktemp)

# Clean up on exit...
trap "rm \"$TEMPFILE\"" 0 1 2 3 15

for file in $@ 
do
    clang-format -style=file $file > $TEMPFILE
    (diff $file $TEMPFILE > /dev/null) || FAILED=""$FAILED" "$file
done
    
if [ "$FAILED" ]
then
    for error in $FAILED
    do
	echo "File $error has formatting discrepency"
    done
    exit 1
fi

exit 0
