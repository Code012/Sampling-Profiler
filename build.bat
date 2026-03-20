@echo off
setlocal

if not exist build mkdir build
pushd build
set cl_common=-nologo -Zi -EHsc -std:c++20
set compile=call cl %cl_common%
set link=-opt:ref -incremental:no
set win32_link=shell32.lib winmm.lib
set target=../profiler.cpp
set output=profile.exe

echo Compiling
%compile% %target% /Fe:%output% /link %win32_link% 

popd