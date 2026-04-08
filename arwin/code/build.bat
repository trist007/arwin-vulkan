@echo off

echo Compilation started at %date% %time%
echo,

REM LIBRARIES
set SDL_LIB=..\arwin\external\SDL3-3.4.2\lib\x64\SDL3.lib
set SDLIMAGE_LIB=..\arwin\external\SDL3_image-3.4.0\lib\x64\SDL3_image.lib
set SDLTTF_LIB=..\arwin\external\SDL3_ttf-3.2.2\lib\x64\SDL3_ttf.lib

REM INCLUDES
set SDL_Include=/I"..\arwin\external\SDL3-3.4.2\include"
set SDLIMAGE_Include=/I"..\arwin\external\SDL3_image-3.4.0\include"
set SDLTTF_Include=/I"..\arwin\external\SDL3_ttf-3.2.2\include"
set FMT_Include=/I"..\arwin\external\fmt-12.1.0\include"

set VULKAN_INCLUDE=/I"..\arwin\code"

set CommonCompilerFlags=/utf-8 /std:c++20 -MT -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4244 -wd4996 -wd4456 -FC -Z7 %SDL_Include% %SDLIMAGE_Include% %SDLTTF_Include% %VULKAN_INCLUDE% %FMT_Include%

set CommonLinkerFlags=-incremental:no -opt:ref /DEBUG /PDB:main.pdb %SDL_LIB% %SDLIMAGE_LIB% %SDLTTF_LIB%

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

cl %CommonCompilerFlags% ..\arwin\code\main.cpp ..\arwin\code\vk_engine.cpp ..\arwin\code\vk_initializers.cpp /link %CommonLinkerFlags%
REM/Fe:win32_arwin.exe

popd

echo.
if %errorlevel% equ 0 (
                       echo Compilation finished at %date% %time%
                       ) else (
                               echo Compilation failed with errors at %date% %time%
                               )
