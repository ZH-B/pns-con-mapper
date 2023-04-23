adb connect 127.0.0.1:7555
adb root

adb install assister/pns-conmapper-assister.apk
adb shell pm grant com.example.pnsconmapperassist android.permission.SYSTEM_ALERT_WINDOW

adb shell \"mkdir -p /data/local/temp\"
adb push build/con-mapper /data/local/temp/
adb shell \"chmod a+x /data/local/temp/con-mapper\"