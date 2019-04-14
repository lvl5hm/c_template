@echo off

pushd build

set compilerFlags=-Od -Gm -MTd -nologo -Oi -GR- -EHa- -WX -W4 -wd4702 -wd4005 -wd4505 -wd4456 -wd4201 -wd4100 -wd4189 -Zi -FC
set linkerFlags=-incremental:no -opt:ref

cl %compilerFlags% ..\code\win32_main.c /link %linkerFlags%

popd