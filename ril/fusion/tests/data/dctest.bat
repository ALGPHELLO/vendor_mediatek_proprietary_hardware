set root=z:\source\ws.g\alps
set project=k63v1_64_bsp
set ver=%1
set filter=%2

IF "%~3"=="" (
    set repeat=1
) ELSE (
    set repeat=%3
)

echo off

adb remount
IF NOT "%~4"=="" (
    echo shuffle=1
)
IF %ver%==32 (
    adb shell rm /data/nativetest/riltest/riltest
)
IF %ver%==64 (
    adb shell rm /data/nativetest64/riltest/riltest
)

IF %ver%==32 (
    adb push %root%\out\target\product\%project%\vendor\lib\librilfusion.so     /system/vendor/lib
)
IF %ver%==64 (
    adb push %root%\out\target\product\%project%\vendor\lib64\librilfusion.so   /system/vendor/lib64
)

IF %ver%==32 (
    adb push %root%\out\target\product\%project%\vendor\lib\libmtk-ril.so     /system/vendor/lib
)
IF %ver%==64 (
    adb push %root%\out\target\product\%project%\vendor\lib64\libmtk-ril.so   /system/vendor/lib64
)

IF %ver%==32 (
    adb push %root%\out\target\product\%project%\data\nativetest\riltest\riltest   /data/nativetest/riltest/riltest
)
IF %ver%==64 (
    adb push %root%\out\target\product\%project%\data\nativetest64\riltest\riltest /data/nativetest64/riltest/riltest
)

IF %ver%==32 (
    adb shell chmod 700 /data/nativetest/riltest/riltest
)
IF %ver%==64 (
    adb shell chmod 700 /data/nativetest64/riltest/riltest
)

IF %ver%==32 (
    IF "%~4"=="" (
        adb shell /data/nativetest/riltest/riltest --gtest_repeat=%repeat%  --gtest_filter=%filter%
    ) ELSE (
        adb shell /data/nativetest/riltest/riltest --gtest_break_on_failure --gtest_repeat=%repeat%  --gtest_filter=%filter% --gtest_shuffle
    )
)
IF %ver%==64 (
    IF "%~4"=="" (
        adb shell /data/nativetest64/riltest/riltest --gtest_repeat=%repeat%  --gtest_filter=%filter%
    ) ELSE (
        adb shell /data/nativetest64/riltest/riltest --gtest_break_on_failure --gtest_repeat=%repeat%  --gtest_filter=%filter% --gtest_shuffle
    )
)

REM PAUSE

