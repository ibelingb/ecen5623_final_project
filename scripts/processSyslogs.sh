#!/bin/bash

if [ $# -eq 0 ]
  then 
    echo "Need PID from last project run." 
    exit -1
fi


mkdir $1
cat /var/log/syslog | grep "project2\[$1\]" > $1/syslog_$1.txt

cat $1/syslog_$1.txt | grep "acquisitionTask" > $1/acqThread_$1.txt &
cat $1/syslog_$1.txt | grep "differenceTask" > $1/diffThread_$1.txt &
cat $1/syslog_$1.txt | grep "processingTask" > $1/procThread_$1.txt &
cat $1/syslog_$1.txt | grep "writeTask" > $1/writeThread_$1.txt &