#!/bin/sh
# release (build) count script
#
DELIMITER=","
PRETEXT=`grep -e "VER_FILEVERSION " version.h | grep -o -e "^.*${DELIMITER}"`
BUILDNUM=`grep -e "VER_FILEVERSION " version.h | grep -o -e "[0-9]*$"`
BUILDNUM=`expr ${BUILDNUM} + 1`

echo "// This file is update by buildcount.sh automatically." > version.h
echo "// Please don't touch." >> version.h
TEXT="${PRETEXT}${BUILDNUM}"
echo "${TEXT}" >> version.h
TEXT=`echo "${TEXT}" | sed -e s/VER_FILEVERSION\ /VER_FILEVERSION_STR\ \"/g`
TEXT=`echo "${TEXT}" | sed -e s/,/./g`
echo "${TEXT}\\0\"" >> version.h

