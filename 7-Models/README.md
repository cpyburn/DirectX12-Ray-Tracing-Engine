coming soon

Notes:

Download assimp latest version

Unzip into Engine root, same path as the all the tutorials
<img width="648" height="452" alt="image" src="https://github.com/user-attachments/assets/57112d66-3202-4324-a09d-3ef19e909cf0" />

cmd into assimp directory. Something like cd C:\Users\Chad\Downloads\assimp-master\assimp-master and cmake as 64 bit application with this command 

cmake -G "Visual Studio 17 2022" -A x64

open Assimp.sln

build all projects with C/C++>Code Generation>

Runtime Library = Multi-threaded Debug DLL (/MDd) ***** OR ***** Runtime Library = Multi-threaded DLL (/MD)

Floating Point Model = fast

IMPORTANT->RUN x64 for all projects both DEBUG and RELEASE

build event post command that copies to output
copy the following dll's to the x64 build debug folder location
\assimp-master\assimp-master\bin\Debug
assimp-vc142-mtd.dll (or latest version)
<img width="1171" height="820" alt="image" src="https://github.com/user-attachments/assets/e8aae8e8-0e59-49a6-9d33-a3356033d58e" />


Also added the following line to gitnore

assimp-master/

In the CPyburnRTXEngine properties>C/C++ General>Additional Include Directories>
Add

..\..\assimp-master\assimp-master\include
