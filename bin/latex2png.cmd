extproc sh
#!/bin/sh
# This version uses latex and dvips
#              with ghostscript
#              and  pnmcrop and pnmtopng
#
help()
{
    cat <<HELP
latex2png -- convert latex file to PNG image

USAGE: latex2png [-d density] [-h] [-k] [-H home dir] latex_file

OPTIONS: -d density (where density is in pixels per inch)
         -h         	this help text
         -k         	keep intermediate files (for debugging)
         -H /home/dir	directory to be included in search path

EXAMPLE: latex2png -d 144 -H /usr/home /tmp/eqn  (to create /tmp/eqn.png)

HELP
    exit 0
}

opt_d=144
opt_k=0
home_dir="."
while [ -n "$1" ]; do
case $1 in
    -h) help;shift 1;;
    -k) opt_k=1;shift 1;; # variable opt_f is set
    -d) opt_d=$2;shift 2;; # -d takes an argument -> shift by 2
    -H) home_dir=$2;shift 2;;
    --) break;;
    -*) echo "error: no such option $1. -h for help";exit 1;;
    *)  break;;
esac
done

# input check:
if [ -z "$1" ] ; then
 echo "ERROR: you must specify a file, use -h for help"
 exit 1
fi

#process 'latex2png file.tex' and 'latex2png file' equivalently
name=`basename "$1" ".tex"`
dir=`dirname "$1"`
start_dir=`pwd`

#need to search the current working directory for \input or \include
cd $home_dir
TEXINPUTS=`pwd`";"
export TEXINPUTS
cd $start_dir
cd $dir
pathname=$dir/$name

rm -f $pathname.png
rm -f $pathname.dvi

LATEX="tex -progname=cslatex -default-translate-file=cp852-cs"

#latex  --interaction batchmode $1.tex > /dev/null

$LATEX --interaction batchmode $pathname > /dev/null

if [ ! -e "$name.dvi" ] ; then
 	echo "ERROR: latex failed to create $name.dvi from $name.tex"
	exit 1
fi

dvips -E -o $name.eps $name.dvi > /dev/null 2>&1

if [ ! -e "$name.eps" ] ; then
 	echo "ERROR: dvips failed to create $name.eps from $name.dvi"
	exit 1
fi

gsos2 -dNOPAUSE -sDEVICE=pbmraw -r${opt_d} -sOutputFile=$name.pbm $name.eps -c quit.ps > /dev/null 2>&1

# pnmcrop $name.pbm | pnmtopng > $name.png 2> /dev/null

if [ "${name%%-*}" = "l2r" ] ; then
        pnm2png -b $name.pbm $name.png $name.off 2> /dev/null
else
        pnm2png $name.pbm $name.png 2> /dev/null       
fi
 
if [ ! -e "$name.png" ] ; then
 	echo "ERROR: gs/pnmcrop/pnmtopng failed to create $name.eps from $name.png"
	exit 1
fi

if [ $opt_k -eq 0 ] ; then
	rm -f $pathname.dvi
	rm -f $pathname.aux
	rm -f $pathname.log
	rm -f $pathname.eps
	rm -f $pathname.pbm
fi

exit 0

