This tutorial is going to add fullscreen capability to the library. For the most part, we are backwards engineering / copy pasteing this [Microsoft DirectX 12 sample project](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Fullscreen) and adding it to ours. You may ask why? It is a good way for us to test fullscreen throughout the building of the engine and I personally think all games should handle resizing of the screen correctly.

### In CPyburnRTXEngine:

In most of our engine classes we will want access to the back buffer count. Easiest way to accomplish this is to make a static variable.

In DeviceResources.h add in public:
```
static constexpr unsigned int c_BackBufferCount = 2;
```
and change the DeviceResources constructor to use c_backBufferCount. Also while we are here we should go ahead and set the constructor up to use ray tracing feature level. So change the minFeatureLevel to default to D3D_FEATURE_LEVEL_12_2. See: [Feature Levels](https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-feature-levels) for more information.  Also want to allow tearing and reverse depth. You can google tearing and HDR settings. Reverse depth is so that our z-buffer/depth buffer has more precision. This is helpful for things like water planes, it stops the shimmering/jerky effects. This will change the clear value in Game.cpp to 0.0f for depth. We will make that change below.
```
        DeviceResources(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
                        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
                        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_2,
                        unsigned int flags = c_AllowTearing | c_ReverseDepth) noexcept(false);
```
Something extra I am going to do, is get completely rid of the m_backBufferCount.  Because we have a public constant, we no longer need the m_backBufferCount. When you download the source code you can see the the removal and see how it is replaced with c_backBufferCount.



Create a class called Fullscreen(.h and .cpp) and put them in the Common filter. I do not like separating headers(.h) and compile files(.cpp). So this is the design pattern I will use going forward. The Fullscreen class will hold most of the code from the example. Some of it will move to new classes that init all things like cbv and srv heaps, graphics root sigs, and graphics piplelines. This will be called GraphicsContexts

Create a class called GraphicsContexts and put the files in Common filter.  This class will need c++ vector, map, mutex, etc. For this to work across all classes we will add these references in the pchlib.h under the other #include <>
```
#include <vector>
#include <map>
#include <mutex>
#include <ppltasks.h>
#include <fstream>
#include <sstream> // istringstream
```

Create a Shaders filter. Select the Shaders filter and in properties set Filter >  hlsl;

In pchlib.h at the very bottom add
```
using Microsoft::WRL::ComPtr;
#include "StepTimer.h"
#include "d3dcompiler.h"

inline void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
{
    if (path == nullptr)
    {
        throw std::exception();
    }

    DWORD size = GetModuleFileName(nullptr, path, pathSize);
    if (size == 0 || size == pathSize)
    {
        // Method failed or path was truncated.
        throw std::exception();
    }

    WCHAR* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
}

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

// Helper for output debug tracing
inline void DebugTrace(_In_z_ _Printf_format_string_ const char* format, ...) noexcept
{
#ifdef _DEBUG
    va_list args;
    va_start(args, format);

    char buff[1024] = {};
    vsprintf_s(buff, format, args);
    OutputDebugStringA(buff);
    va_end(args);
#else
    UNREFERENCED_PARAMETER(format);
#endif
}
```
Copy postShaders.hlsl and sceneShaders.hlsl from the Microsoft Sample into the CPyburnRTXEngine and include in project. The location should be something like this:

DirectX-Graphics-Samples-master\DirectX-Graphics-Samples-master\Samples\Desktop\D3D12Fullscreen\src

You can do all three of these steps at once:
- Select both files > right click > Properties > Item Type > Custome Build Tool 
- Select both files > right click > Properties > General > Command Line > copy %(Identity) "$(OutDir)" > NUL 
- Select both files > right click > Properties > General > Outputs > $(OutDir)\%(Identity) > OK

Right click CpyburnRTXEngine > Properties > Librarian > General > Additional Dependencies > Edit > Add d3dcompiler.lib > OK > OK

### In TestGame:
Game.h add
```
#include <Fullscreen.h>
```
and in private add
```
// fullscreen
CPyburnRTXEngine::Fullscreen                  m_fullscreen;
```
and change 
```
// Device resources.
std::unique_ptr<DX::DeviceResources>        m_deviceResources;
```
to
```
// Device resources.
std::shared_ptr<DX::DeviceResources>        m_deviceResources;
```
In Game.cpp:
change the following code in Game::Game() noexcept(false)
```
m_deviceResources = std::make_unique<DX::DeviceResources>();
```
to
```
m_deviceResources = std::make_shared<DX::DeviceResources>();
```
add the following code in void Game::CreateDeviceDependentResources()
```
m_fullscreen.CreateDeviceDependentResources(m_deviceResources);
```
change
```
commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
```
to
```
commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
```
