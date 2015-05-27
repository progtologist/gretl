#!/bin/sh

INDEX=nls_main.shtml
INDEX_URL="http://www.itl.nist.gov/div898/strd/nls/nls_main.shtml"
DATADIR="http://itl.nist.gov/div898/strd/nls/data/LINKS/DATA"

# Data file example:
# http://itl.nist.gov/div898/strd/nls/data/LINKS/DATA/Misra1a.dat
if [ -f ./$INDEX ] ;  then
	echo "$INDEX already present"
else
	wget $INDEX_URL
fi
for datafile in \
`grep shtml $INDEX | grep data | awk -F'/' '{print $2}' \
  | awk -F'>' '{ print $NF }' | awk -F'<' '{ print $1 }'` ; do
  if [ -f ./$datafile.dat ] ; then
     echo "$datafile.dat already present"
  else 
    echo "getting $datafile.dat"
    wget $DATADIR/$datafile.dat
  fi
done
