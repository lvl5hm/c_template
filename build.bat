@echo off

pushd build

set compilerFlags=-Od -MTd -nologo -Oi -GR- -EHa- -WX -W4 -wd4702 -wd4005 -wd4505 -wd4101 -wd4456 -wd4201 -wd4100 -wd4189 -wd4204 -wd4459 -Zi -FC -I"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.22.27905\include" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.17763.0\ucrt" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.17763.0\um" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.17763.0\shared"
set compilerFlags=-O2 -MTd -nologo -Oi -GR- -EHa- -WX -W4 -wd4702 -wd4005 -wd4505 -wd4101 -wd4456 -wd4201 -wd4100 -wd4189 -wd4204 -wd4459 -Zi -FC -I"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.22.27905\include" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.17763.0\ucrt" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.17763.0\um" -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.17763.0\shared"

set linkerFlags=-incremental:no -opt:ref OpenGL32.lib Winmm.lib user32.lib Gdi32.lib /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.22.27905\lib\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\ucrt\x64"



del game*.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp

cl %compilerFlags% /LD ..\code\game.c /link %linkerFlags% /out:game.dll /EXPORT:game_update -PDB:game_%random%.pdb

del lock.tmp

cl %compilerFlags% ..\code\win32_main.c /link %linkerFlags%

popd