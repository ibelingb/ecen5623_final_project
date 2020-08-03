#!/bin/bash

if [ $# -eq 0 ]
  then 
    echo "Need PID from last project run." 
    exit -1
fi


mkdir $1
cat /var/log/syslog | grep "project\[$1\]" > $1/syslog_$1.txt

cat $1/syslog_$1.txt | grep "acquisitionTask frame process start" > $1/acqThread_start_$1.txt &
cat $1/syslog_$1.txt | grep "acquisitionTask frame" > $1/acqThread_ACET_$1.txt &
cat $1/syslog_$1.txt | grep "acquisitionTask frame inserted" > $1/acqThread_finish_$1.txt &

cat $1/syslog_$1.txt | grep "differenceTask frame process start" > $1/diffThread_start_$1.txt &
cat $1/syslog_$1.txt | grep "differenceTask frame" > $1/diffThread__ACET_$1.txt &
cat $1/syslog_$1.txt | grep "differenceTask frame #" > $1/diffThread__finish_$1.txt &

cat $1/syslog_$1.txt | grep "processingTask frame process start" > $1/procThread_start_$1.txt &
cat $1/syslog_$1.txt | grep "processingTask frame" > $1/procThread_ACET_$1.txt &
cat $1/syslog_$1.txt | grep "processingTask frame #" > $1/procThread_finish_$1.txt &

cat $1/syslog_$1.txt | grep "writeTask frame process start" > $1/writeThread_start_$1.txt &
cat $1/syslog_$1.txt | grep "writeTask frame" > $1/writeThread_ACET_$1.txt &
cat $1/syslog_$1.txt | grep "writeTask frame #" > $1/writeThread_finish_$1.txt &
