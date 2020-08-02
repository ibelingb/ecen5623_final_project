#!/bin/bash

if [ $# -eq 0 ]
  then 
    echo "Need PID from last project run." 
    exit -1
fi


mkdir $1
cat /var/log/syslog | grep "project\[$1\]" > $1/syslog_$1.txt

cat $1/syslog_$1.txt | grep "acquisitionTask frame" > $1/acqThread_$1.txt &
cat $1/syslog_$1.txt | grep "differenceTask frame" > $1/diffThread_$1.txt &
cat $1/syslog_$1.txt | grep "processingTask frame" > $1/procThread_$1.txt &
cat $1/syslog_$1.txt | grep "writeTask frame" > $1/writeThread_$1.txt &
