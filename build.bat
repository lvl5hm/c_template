@echo off

pushd build

set compilerFlags=-Od -MTd -nologo -Oi -GR- -EHa- -WX -W4 -wd4702 -wd4005 -wd4505 -wd4101 -wd4456 -wd4201 -wd4100 -wd4189 -wd4204 -wd4459 -Zi -FC

set linkerFlags=-incremental:no -opt:ref OpenGL32.lib Winmm.lib user32.lib Gdi32.lib



del game*.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.tmp

cl %compilerFlags% /LD ..\code\game.c /link %linkerFlags% /out:game.dll /EXPORT:game_update -PDB:game_%random%.pdb

del lock.tmp

cl %compilerFlags% ..\code\win32_main.c /link %linkerFlags%

popd