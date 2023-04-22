PROJECT=con-mapper

alias m='
make

if [ $? -eq 0 ] 
then
    echo BUILD SUCCESS
else
    echo BUILD FAILED
fi
'

alias M='
make clean; m
'

alias push="
adb connect 127.0.0.1:7555
adb shell \"mkdir -p /data/local/temp\"
adb push build/$PROJECT /data/local/temp
adb shell \"chmod a+x /data/local/temp/$PROJECT\"
"

PKG_NAME='com.example.pnsconmapperassist'
PIP_NAME="/data/local/temp/con_mapper_ui.fifo"

alias run="
    adb shell \"/data/local/temp/$PROJECT\"
"

alias stop="
    adb shell killall com.example.pnsconmapperassist
    adb shell killall screenrecord
    adb shell killall con-mapper
"