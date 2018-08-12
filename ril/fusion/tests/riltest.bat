
set root=Z:\N
set project=k55v1_64_om_c2k6m
set repeat=1
set filter=FastDormancyTest.*

adb remount

adb shell rm /data/nativetest/riltest/riltest
adb shell rm /data/nativetest64/riltest/riltest

adb push %root%\out\target\product\%project%\system\vendor\lib\librilfusion.so     /system/vendor/lib
adb push %root%\out\target\product\%project%\system\vendor\lib64\librilfusion.so   /system/vendor/lib64

adb push %root%\out\target\product\%project%\system\vendor\lib\libmtk-ril.so     /system/vendor/lib
adb push %root%\out\target\product\%project%\system\vendor\lib64\libmtk-ril.so   /system/vendor/lib64

adb push %root%\out\target\product\%project%\data\nativetest\riltest\riltest   /data/nativetest/riltest/riltest
adb push %root%\out\target\product\%project%\data\nativetest64\riltest\riltest /data/nativetest64/riltest/riltest

adb shell chmod 700 /data/nativetest/riltest/riltest
adb shell chmod 700 /data/nativetest64/riltest/riltest

adb shell /data/nativetest/riltest/riltest   --gtest_repeat=%repeat%  --gtest_filter=%filter%
adb shell /data/nativetest64/riltest/riltest --gtest_repeat=%repeat%  --gtest_filter=%filter%
PAUSE

