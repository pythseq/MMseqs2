#!/bin/sh
# Clustering workflow script
checkReturnCode () { 
	[ $? -ne 0 ] && echo "$1" && exit 1;
}
notExists () { 
	[ ! -f "$1" ] 
}
#pre processing
[ -z "$MMDIR" ] && echo "Please set the environment variable $MMDIR to your MMSEQS installation directory." && exit 1;
# check amount of input variables
[ "$#" -ne 4 ] && echo "Please provide <queryDB> <targetDB> <outDB> <tmp>" && exit 1;
# check if files exists
[ ! -f "$1" ] &&  echo "$1 not found!" && exit 1;
[ ! -f "$2" ] &&  echo "$2 not found!" && exit 1;
[   -f "$3" ] &&  echo "$3 exsists already!" && exit 1;
[ ! -d "$4" ] &&  echo "tmp directory $4 not found!" && exit 1;

# processing
# call prefilter module
notExists "$4/pref" && mmseqs prefilter "$1" "$2" "$4/pref" $PREFILTER_PAR           && checkReturnCode "Prefilter died"
# call alignment module
notExists "$4/aln"  && mmseqs alignment "$1" "$2" "$4/pref" "$4/aln" $ALIGNMENT_PAR  && checkReturnCode "Alignment died"

# post processing
cp "$4/aln" "$3"
cp "$4/aln.index" "$3.index"
checkReturnCode "Could not copy result to $3"

[ -z "$KEEP_TEMP" ] && echo "Keeping temporary files" && exit 0
rm -f "$4"/*
