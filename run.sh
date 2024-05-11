#!/bin/bash

exe=""
np=4
cc="composition_cluster"


function usage(){
	echo "Usage ./run.sh"
	echo "	./run.sh [OPTIONS] nom_executable"
	echo ""
	echo "OPTIONS:"
	echo "	-n valeur:"
	echo "		Configure le nombre de processeur utilisé" 
	echo "		(default=4)"
	echo ""
	echo "	-c valeur:"
	echo "		Configure le chemin du fichier composition du cluster"
	echo "		(default="composition_cluster")"
	exit
}

while getopts "n:m:" opt; do
  case $opt in
    n) np=$OPTARG ;;
    c) cc=$OPTARG ;;
    \?) usage
        ;;
  esac
done

shift $((OPTIND-1))

if [ -n "$1" ]; then
  exe=$1
else
  usage
fi

mpirun -np $np -hostfile $cc -mca btl_tcp_if_include enp0s31f6 $@
