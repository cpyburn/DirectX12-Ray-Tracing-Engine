Download the latest [directx vs template](https://github.com/walbourn/directx-vs-templates/wiki) for Visual Studio and follow the wiki directions to create a new project.

We want to create a Direct3D12 Win32 Game DR project
<img width="1627" height="619" alt="dx12project" src="https://github.com/user-attachments/assets/07d2efcf-a179-4210-8dfd-1b51696a484b" />

Going to name the solution CPyburnRTXEngine. Going to name the game TestGame. Make sure it is x64. Hit play.
<img width="688" height="120" alt="Screenshot 2025-10-18 080833" src="https://github.com/user-attachments/assets/eb49e53e-ce76-44f5-b7e2-f6b7ed9741b9" />

You should get a cornflower blue screen.
<img width="802" height="632" alt="image" src="https://github.com/user-attachments/assets/d9a4dacc-c8fa-4b11-936a-59874874908d" />

At this point we have a working game but it isn't going to be super reusable for all our purposes.  We want a .lib that we can reference from any game project with all the methods we will create to make our engine.

Time to create our Static Library. Right click on the solution. Add new project. Type static library in the search. Add the Static Library and name it CPyburnRTXEngine
<img width="1111" height="193" alt="image" src="https://github.com/user-attachments/assets/9909e657-ee79-4eb6-920e-619626e9f648" />

Once it is added right click on the CPyburnRTXEngine project and select properties. Now pick c++ Language Standard and select ISO C++20 Standard.

Do the same for the TestGame project.

Now show all files for both projects. 

<img width="331" height="121" alt="image" src="https://github.com/user-attachments/assets/b1945bf4-b3b3-4b15-afd9-7c0688331930" />

Move DeviceResources.h, DeviceResources.cpp, StepTimer.h from TestGame up to CPyburnRTXEngine library.

Right click on CPyburnRTXEngine > properties > Librarian > General Additional Dependencies > d3d12.lib;dxgi.lib;dxguid.lib; > OK

Right click on CPyburnRTXEngine > properties > C/C++ > Precompiled headers > Precompiled Header File > pchlib.h > OK

Rename pch.h in CPyburnRTXEngine to pchlib.h

Rename pch.cpp in CPyburnRTXEngine to pchlib.cpp

Edit pchlib.cpp and change
```
// pch.cpp: source file corresponding to the pre-compiled header

#include "pch.h"

// When you are using pre-compiled headers, this source file is necessary for compilation to succeed.
```
to
```
// pch.cpp: source file corresponding to the pre-compiled header

#include "pchlib.h"

// When you are using pre-compiled headers, this source file is necessary for compilation to succeed.
```
Edit DeviceResources.cpp and change
```
#include "pch.h"
#include "DeviceResources.h"
```
to
```
#include "pchlib.h"
#include "DeviceResources.h"
```

Copy and paste the entire contents pch.h file from TestGame into CPyburnRTXEngine pchlib.h

Right click on CPyburnRTXEngine project and build

Right click on TestGame > Add > Reference > CPyburnRTXEngine > Ok

Right click on TestGame > Properties > C/C++ > General > Additional Include Directories > Edit > *folder > ..\CPyburnRTXEngine\ > OK

Now delete everything in TestGame pch.h and paste below into the  pch.h file
```
//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <pchlib.h>
```
In Game.h change 
```
#include "DeviceResources.h"
#include "StepTimer.h"

#include <memory>
```
to
```
#include <DeviceResources.h>
#include <StepTimer.h>
```

A little clean up. Delete farmework.h and CPyburnRTXEngine.cpp from CPyburnRTXEngine
