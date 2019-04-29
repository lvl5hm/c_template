@echo off

pushd build

set compilerFlags=-Od -Gm -MTd -nologo -Oi -GR- -EHa- -WX -W4 -wd4702 -wd4005 -wd4505 -wd4456 -wd4201 -wd4100 -wd4189 -wd4204 -Zi -FC
set linkerFlags=-incremental:no -opt:ref OpenGL32.lib Winmm.lib user32.lib Gdi32.lib

rem dsound.lib

cl %compilerFlags% ..\code\win32_main.c /link %linkerFlags%

cl %compilerFlags% /LD ..\code\game.c /link -incremental:no /out:game.dll /EXPORT:game_update

popd