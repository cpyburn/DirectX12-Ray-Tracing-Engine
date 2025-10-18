Download the latest [directx vs template](https://github.com/walbourn/directx-vs-templates/wiki) for Visual Studio and follow the wiki directions to create a new project.

We want to create a Direct3D12 Win32 Game DR project
<img width="1627" height="619" alt="dx12project" src="https://github.com/user-attachments/assets/07d2efcf-a179-4210-8dfd-1b51696a484b" />

Going to name the solution CPyburnRTXEngine. Going to name the game TestGame. Make sure it is x64. Hit play.
<img width="688" height="120" alt="Screenshot 2025-10-18 080833" src="https://github.com/user-attachments/assets/eb49e53e-ce76-44f5-b7e2-f6b7ed9741b9" />

You should get a cornflower blue screen.
<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/d9a4dacc-c8fa-4b11-936a-59874874908d" />

At this point we have a working game but it isn't going to be super reusable for all our purposes.  We want a .lib that we can reference from any game project with all the methods we will create to make our engine.  It is a little intense going back and forth from game to library if you have never done anything like this before.

Time to create our Static Library. Right click on the solution. Add new project. Type static library in the search. Add the Static Library and name it CPyburnRTXEngine
<img width="1111" height="193" alt="image" src="https://github.com/user-attachments/assets/9909e657-ee79-4eb6-920e-619626e9f648" />

Once it is added right click on the CPyburnRTXEngine project and select properties. Now pick c++ Language Standard and select ISO C++20 Standard.
Do the same for the TestGame project.

Now show all files for both projects. 
<img width="331" height="121" alt="image" src="https://github.com/user-attachments/assets/b1945bf4-b3b3-4b15-afd9-7c0688331930" />

Move DeviceResources.h, DeviceResources.cpp, StepTimer.h from TestGame up to CPyburnRTXEngine library.
Right click on CPyburnRTXEngine > properties > Librarian > General Additional Dependencies > d3d12.lib;dxgi.lib;dxguid.lib;
uuid.lib;kernel32.lib;user32.lib;comdlg32.lib;advapi32.lib;shell32.lib;ole32.lib;oleaut32.lib;runtimeobject.lib;%(AdditionalDependencies)

Copy and paste the entire contents pch.h file from TestGame into CPyburnRTXEngine pch.h
Now copy and paste below into the TestGame pch.h file
```
//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <pch.h>
//using namespace CPyburnRTXEngine;
```
