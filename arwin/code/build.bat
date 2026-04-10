@echo off

echo Compilation started at %date% %time%
echo,

REM capture start time
set START_TIME=%time%

REM increment build counter
set COUNTER_FILE=build_count.txt
if not exist %COUNTER_FILE% echo 0 > %COUNTER_FILE%
set /p BUILD_COUNT=<%COUNTER_FILE%
set /a BUILD_COUNT+=1
echo %BUILD_COUNT% > %COUNTER_FILE%
echo BUILD #%BUILD_COUNT%

REM LIBRARIES
set SDL_LIB=..\arwin\external\SDL3-3.4.2\lib\x64\SDL3.lib
set SDLIMAGE_LIB=..\arwin\external\SDL3_image-3.4.0\lib\x64\SDL3_image.lib
set SDLTTF_LIB=..\arwin\external\SDL3_ttf-3.2.2\lib\x64\SDL3_ttf.lib
set VULKAN_LIB=..\arwin\code\vulkan-1.lib

REM INCLUDES
set SDL_Include=/I"..\arwin\external\SDL3-3.4.2\include"
set SDLIMAGE_Include=/I"..\arwin\external\SDL3_image-3.4.0\include"
set SDLTTF_Include=/I"..\arwin\external\SDL3_ttf-3.2.2\include"
set FASTGLTF_Includes=/I"..\arwin\external\fastgltf-0.6.0\include"
set GLM_Includes=/I"..\arwin\external\glm-1.0.3"
REM set FMT_Include=/I"..\arwin\external\fmt-12.1.0\include" // can just use std::format in c++20

set VULKAN_INCLUDE=/I"..\arwin\code"

REM for debug /DDEBUG enabled the DEBUG define

REM -MT can't use -MT which statically links to the CRT but Vulkan Validation layer has a conflict and uses a different
REM CRT so need to use /MD to dynamically link 
REM had to remove -EHa- /EHsc enabled exception handling but -EHa- disables it
REM -WX is warnings as errors to be safe, if warning is ok I will suppress it with -wd<warning_no>
set CommonCompilerFlags=/utf-8 /std:c++20 /EHsc ^
    /MD -nologo -fp:fast -Gm- -Od -Oi -WX -W4 ^
    /Zc:threadSafeInit- ^
    /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR ^
    -wd4202 -wd4100 -wd4189 -wd4244 -wd4996 -wd4456 -wd4324 -FC -Z7 ^
    %SDL_Include% %SDLIMAGE_Include% %SDLTTF_Include% %VULKAN_INCLUDE% %FASTGLTF_Includes% %GLM_Includes% ^
    /DGLM_ENABLE_EXPERIMENTAL

set CommonLinkerFlags=-incremental:no -opt:ref /DEBUG /PDB:main.pdb %SDL_LIB% %SDLIMAGE_LIB% %SDLTTF_LIB% %VULKAN_LIB%

REM echo Updating etags
REM echo,
REM etags *.cpp *.h raylib\*.c

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
copy /Y ..\arwin\external\SDL3-3.4.2\lib\x64\SDL3.dll . > NUL
copy /Y ..\arwin\external\SDL3_image-3.4.0\lib\x64\SDL3_image.dll . > NUL
copy /Y ..\arwin\external\SDL3_ttf-3.2.2\lib\x64\SDL3_ttf.dll . > NUL

REM delete pdb because debugger maintains a lock on pdb so pdb cannot be overwritten
del *.pdb > NUL 2> NUL

cl %CommonCompilerFlags% -wd4715 -wd4267 -wd4458 ^
    ..\arwin\external\fastgltf-0.6.0\src\fastgltf.cpp ^
    /c /Fofastgltf.obj

cl %CommonCompilerFlags% -wd4715 -wd4267 -wd4458 ^
    ..\arwin\external\fastgltf-0.6.0\src\base64.cpp ^
    /c /Fobase64.obj

cl %CommonCompilerFlags% -wd4127 -wd4505 -wd4715 -wd4267 -wd4458 ^
    ..\arwin\code\simdjson.cpp ^
    /c /Fosimdjson.obj

cl %CommonCompilerFlags% ^
    ..\arwin\code\main.cpp ^
    ..\arwin\code\vk_engine.cpp ^
    ..\arwin\code\vk_initializers.cpp ^
    ..\arwin\code\vk_images.cpp ^
    ..\arwin\code\VkBootstrap.cpp ^
    ..\arwin\code\vk_descriptors.cpp ^
    ..\arwin\code\vk_pipelines.cpp ^
    ..\arwin\code\vk_loader.cpp ^
    ..\arwin\code\imgui.cpp ^
    ..\arwin\code\imgui_demo.cpp ^
    ..\arwin\code\imgui_draw.cpp ^
    ..\arwin\code\imgui_tables.cpp ^
    ..\arwin\code\imgui_widgets.cpp ^
    ..\arwin\code\imgui_impl_sdl3.cpp ^
    ..\arwin\code\imgui_impl_vulkan.cpp ^
    fastgltf.obj base64.obj simdjson.obj ^
    /link %CommonLinkerFlags%
REM/Fe:win32_arwin.exe

popd

REM capture end time and calculate the difference
set END_TIME=%time%

REM parse start time
for /f "tokens=1-4 delims=:., " %%a in ("%START_TIME%") do (
    set /a START_H=%%a
    set /a START_M=%%b
    set /a START_S=%%c
    set /a START_CS=%%d
)

REM parse end time
for /f "tokens=1-4 delims=:., " %%a in ("%END_TIME%") do (
    set /a END_H=%%a
    set /a END_M=%%b
    set /a END_S=%%c
    set /a END_CS=%%d
)

REM convert to centiseconds and subtract
set /a START_TOTAL=(START_H*360000)+(START_M*6000)+(START_S*100)+START_CS
set /a END_TOTAL=(END_H*360000)+(END_M*6000)+(END_S*100)+END_CS
set /a DIFF=END_TOTAL-START_TOTAL

REM convert back to minutes and seconds
set /a DIFF_M=DIFF/6000
set /a DIFF_S=(DIFF%%6000)/100
set /a DIFF_CS=DIFF%%100

echo.
if %errorlevel% equ 0 (
                       echo Compilation finished at %date% %time%
                       echo Build time: %DIFF_M%m %DIFF_S%.%DIFF_CS%s
                       ) else (
                               echo Compilation failed with errors at %date% %time%
                               echo Build time: %DIFF_M%m %DIFF_S%.%DIFF_CS%s
                               )
