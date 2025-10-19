This tutorial is going to add fullscreen capability to the library. For the most part, we are backwards engineering / copy pasteing this [Microsoft DirectX 12 sample project](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Fullscreen) and adding it to ours. You may ask why? It is a good way for us to test fullscreen throughout the building of the engine and I personally think all games should handle resizing of the screen correctly.

In CPyburnRTXEngine:

Create a class called Fullscreen and put it in the Common filter

Create a Shaders filter

In fitler properties add hlsl;

In pchlib.h at the very bottom add
```
#include "StepTimer.h"
```
Show all files for CPyburnRTXEngine and add postShaders.hlsl and sceneShaders.hlsl from the Microsoft Sample. The location should be something like this:

DirectX-Graphics-Samples-master\DirectX-Graphics-Samples-master\Samples\Desktop\D3D12Fullscreen\src
