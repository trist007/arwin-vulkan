@echo off

echo Compilation started at %date% %time%
echo,

REM capture start time
set START_TIME=%time%
set START_TIME=%START_TIME: =0%

REM increment build counter
set COUNTER_FILE=build_count.txt
if not exist %COUNTER_FILE% echo 0 > %COUNTER_FILE%
set /p BUILD_COUNT=<%COUNTER_FILE%
set /a BUILD_COUNT+=1
echo %BUILD_COUNT% > %COUNTER_FILE%
echo BUILD #%BUILD_COUNT%

echo cwd: %CD%
set VULKAN_SDK_PATH=C:\VulkanSDK\1.4.341.1\

REM LIBRARIES
set VULKAN_LIB=%VULKAN_SDK_PATH%\Lib\vulkan-1.lib
set SDL3_LIB=%VULKAN_SDK_PATH%\Lib\SDL3.lib
set SLANG_LIB=%VULKAN_SDK_PATH%\Lib\slang.lib

REM INCLUDES
set VULKAN_SDK_INCLUDES=/I%VULKAN_SDK_PATH%\Include
set INCLUDES=/I.

REM for debug /DDEBUG enabled the DEBUG define

REM -MT can't use -MT which statically links to the CRT but Vulkan Validation layer has a conflict and uses a different
REM CRT so need to use /MD to dynamically link 
REM had to remove -EHa- /EHsc enabled exception handling but -EHa- disables it
REM -WX is warnings as errors to be safe, if warning is ok I will suppress it with -wd<warning_no>
set CommonCompilerFlags=/utf-8 /std:c++20 /EHsc ^
    /MD -nologo -fp:fast -Gm- -Od -Oi -WX -W4 ^
    /Zc:threadSafeInit- ^
    /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR ^
    -wd4202 -wd4100 -wd4189 -wd4244 -wd4996 -wd4456 -wd4324 -wd4505 -FC -Z7 ^
    %VULKAN_SDK_INCLUDES% %INCLUDES%

set CommonLinkerFlags=-incremental:no -opt:ref /DEBUG /PDB:main.pdb %VULKAN_LIB% %SDL3_LIB% %SLANG_LIB%

REM echo Updating etags
REM echo,
REM etags *.cpp *.h raylib\*.c

IF NOT EXIST ..\bin mkdir ..\bin
pushd ..\bin

REM delete pdb because debugger maintains a lock on pdb so pdb cannot be overwritten
del *.pdb > NUL 2> NUL


cl %CommonCompilerFlags% ^
    ..\code\main.cpp ^
    ..\code\arena.cpp ^
    ..\code\initVulkan.cpp ^
    ..\code\vk_engine.cpp ^
    ..\code\vk_initializers.cpp ^
    ..\code\vk_pipelines.cpp ^
    ..\code\vk_loader.cpp ^
    ..\code\camera.cpp ^
    /link %CommonLinkerFlags%

REM/Fe:win32_arwin.exe

    REM ==================== ImGui (temporarily disabled) ====================
    REM ..\arwin\code\imgui.cpp ^
    REM ..\arwin\code\imgui_demo.cpp ^
    REM ..\arwin\code\imgui_draw.cpp ^
    REM ..\arwin\code\imgui_tables.cpp ^
    REM ..\arwin\code\imgui_widgets.cpp ^
    REM ..\arwin\code\imgui_impl_sdl3.cpp ^
    REM ..\arwin\code\imgui_impl_vulkan.cpp ^

popd

REM capture end time and calculate the difference
set END_TIME=%time%
set END_TIME=%END_TIME: =0%

REM Remove any leading zeros safely for calculation
set START_H=%START_TIME:~0,2%
set START_M=%START_TIME:~3,2%
set START_S=%START_TIME:~6,2%
set START_MS=%START_TIME:~9,2%

set END_H=%END_TIME:~0,2%
set END_M=%END_TIME:~3,2%
set END_S=%END_TIME:~6,2%
set END_MS=%END_TIME:~9,2%

REM Convert to total seconds
set /a START_TOTAL=(((START_H*60)+START_M)*60)+START_S
set /a END_TOTAL=(((END_H*60)+END_M)*60)+END_S

set /a TOTAL_SEC=%END_TOTAL% - %START_TOTAL%
if %TOTAL_SEC% LSS 0 set /a TOTAL_SEC+=86400

REM Milliseconds
set /a TOTAL_MS=%END_MS% - %START_MS%
if %TOTAL_MS% LSS 0 (
    set /a TOTAL_MS+=1000
    set /a TOTAL_SEC-=1
)

if %TOTAL_MS% LSS 10 set TOTAL_MS=0%TOTAL_MS%


echo.
if %errorlevel% equ 0 (
                       echo Compilation finished at %date% %time%
                       ) else (
                               echo Compilation failed with errors at %date% %time%
                               )

echo ================================================
echo Build #%BUILD_COUNT% completed in %TOTAL_SEC%.%TOTAL_MS%s