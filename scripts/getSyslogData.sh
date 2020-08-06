#!/bin/bash

if [ $# -eq 0 ]
  then 
    echo "Need PID from last project run." 
    exit -1
fi

mkdir -p /E/proj_data/$1/logs /E/proj_data/$1/frames

ssh -i ~/.ssh/rpi pi@raspberrypi "cd ~/proj/scripts/; sudo ./processSyslogs.sh $1"

echo "Waiting for processSyslogs.sh to complete..."
sleep 3

scp -r  -i ~/.ssh/rpi pi@raspberrypi:~/proj/scripts/$1/* /E/proj_data/$1/logs
scp -r  -i ~/.ssh/rpi pi@raspberrypi:~/proj/f* /E/proj_data/$1/frames
scp -r  -i ~/.ssh/rpi pi@raspberrypi:~/proj/video.avi /E/proj_data/$1
scp -r  -i ~/.ssh/rpi pi@raspberrypi:~/proj/output.txt /E/proj_data/$1

exit 0
