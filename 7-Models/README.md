coming soon

Notes:
unzip into Engine root, same path as the BuildProcessTemplates
assimp as follows;
cmd into assimp and cmake as 64 bit application 
cmake -G "Visual Studio 17 2022" -A x64
Assimp.sln

build all projects with c++ code generation as follows;
Runtime Library = Multi-threaded Debug DLL (/MDd) ***** OR ***** Runtime Library = Multi-threaded DLL (/MD)
Floating Point Model = fast
IMPORTANT->RUN x64 for all projects both DEBUG and RELEASE

[OBSOLETE] replaced with build event post command that copies to output
copy the following dll's to the x64 build debug folder location
\assimp-master\assimp-master\bin\Debug
assimp-vc142-mtd.dll (or latest version)
