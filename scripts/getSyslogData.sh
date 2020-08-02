#!/bin/bash

if [ $# -eq 0 ]
  then 
    echo "Need PID from last project run." 
    exit -1
fi

mkdir -p /C/proj_data/$1/logs /C/proj_data/$1/frames

ssh -i ~/.ssh/rpi pi@raspberrypi "cd ~/proj/scripts/; sudo ./processSyslogs.sh $1"

echo "Waiting for processSyslogs.sh to complete..."
sleep 3

scp -r  -i ~/.ssh/rpi pi@raspberrypi:~/proj/scripts/$1/* /C/proj_data/$1/logs
scp -r  -i ~/.ssh/rpi pi@raspberrypi:~/proj/f* /C/proj_data/$1/frames
scp -r  -i ~/.ssh/rpi pi@raspberrypi:~/proj/video.avi /C/proj_data/$1

exit 0
