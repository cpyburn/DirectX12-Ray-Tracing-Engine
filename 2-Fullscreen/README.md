This tutorial is going to add fullscreen capability to the library. For the most part, we are backwards engineering / copy pasteing this [Microsoft DirectX 12 sample project](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Fullscreen) and adding it to ours. You may ask why? It is a good way for us to test fullscreen throughout the building of the engine and I personally think all games should handle resizing of the screen correctly.

The main things to take away from here are the addition and refactoring of the following clases.

1.	GraphicsContexts
2.	ConstantBuffer
3.	FrameResource
4.	TestFullscreen (rasterization)

# GraphicsContexts.cpp — Descriptor Heap Management (DirectX 12)

This file implements **global descriptor heap management** for a DirectX 12 renderer.
Its primary responsibility is to **allocate, reuse, and track descriptor heap indices** for CBV/SRV/UAV descriptors in a **thread-safe** way.

It also creates and owns a **shader-visible descriptor heap** that is shared across graphics contexts.

---

## High-Level Responsibilities

`GraphicsContexts` handles:

* Creation of a **shader-visible CBV/SRV/UAV descriptor heap**
* Tracking the **descriptor size** for handle offset calculations
* Allocating descriptor heap indices
* Reclaiming and reusing freed indices
* Supporting **multi-use heap positions** (reference counted)
* Ensuring **thread safety** during heap position allocation and reclamation

This is essentially a **descriptor index allocator** layered on top of a single global descriptor heap.

---

## Static State Overview

The class is implemented using **static members**, meaning all graphics contexts share the same heap and allocation state.

```cpp
UINT GraphicsContexts::c_descriptorSize;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GraphicsContexts::c_heap;
```

* `c_descriptorSize`
  Stores the descriptor handle increment size returned by the D3D12 device.

* `c_heap`
  The global shader-visible CBV/SRV/UAV descriptor heap.

---

### Heap Allocation Tracking

```cpp
UINT GraphicsContexts::m_heapPositionCounter;
vector<UINT> GraphicsContexts::m_availableHeapPositions;
map<UINT, UINT> GraphicsContexts::m_multiUseHeapPositions;
mutex GraphicsContexts::m_mutexMultiUseHeapPositions;
```

* `m_heapPositionCounter`
  Monotonically increasing counter for new heap indices.

* `m_availableHeapPositions`
  Pool of freed descriptor indices that can be reused.

* `m_multiUseHeapPositions`
  Reference count map for heap positions that are shared by multiple resources.

* `m_mutexMultiUseHeapPositions`
  Ensures thread-safe access to all heap tracking structures.

---

## Descriptor Allocation Flow

### GetAvailableHeapPosition()

Returns a descriptor heap index that can be safely used.

```cpp
UINT GraphicsContexts::GetAvailableHeapPosition()
```

Allocation logic:

1. If a reclaimed index exists → reuse it
2. Otherwise → allocate a new index by incrementing `m_heapPositionCounter`
3. Asserts if the counter reaches `UINT32_MAX`

This prevents descriptor exhaustion while favoring reuse.

---

## Multi-Use Descriptor Support

Some descriptors (e.g. shared textures or constant buffers) may be used by **multiple objects**.

### AddMultiHeapPosition()

```cpp
void GraphicsContexts::AddMultiHeapPosition(UINT heapPosition)
```

* Increments a reference count for a heap position
* Creates an entry if it doesn’t exist
* Thread-safe via mutex

---

### RemoveHeapPosition()

```cpp
bool GraphicsContexts::RemoveHeapPosition(UINT heapPosition)
```

Handles both single-use and multi-use descriptors:

* If multi-use:

  * Decrements reference count
  * Reclaims the heap position only when count reaches zero
* If single-use:

  * Immediately returns the index to the available pool

Returns `true` if the heap position was fully reclaimed.

---

## Descriptor Heap Creation

### CreateDeviceDependentResources()

```cpp
void GraphicsContexts::CreateDeviceDependentResources(ID3D12Device* d3dDevice)
```

This method creates the global descriptor heap and must be called **after device creation**.

Key steps:

1. Query descriptor handle increment size
2. Create a shader-visible CBV/SRV/UAV heap
3. Store it globally for use by all graphics contexts

```cpp
heapDesc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
```

The heap size is currently:

```cpp
DeviceResources::c_backBufferCount + 1
```

> ⚠️ The `+1` is an intentionally arbitrary value and should be expanded as the engine grows.

---

## Thread Safety

All operations that mutate heap state are guarded by:

```cpp
std::mutex m_mutexMultiUseHeapPositions;
```

This allows descriptor allocation and reclamation from **multiple threads** without corruption or double-free issues.

---

## Summary

This file provides a **centralized, thread-safe descriptor heap allocator** for a DirectX 12 engine:

* One global shader-visible descriptor heap
* Reusable descriptor indices
* Reference-counted shared descriptors
* Safe for multi-threaded rendering workflows

It abstracts away the complexity of descriptor management while ensuring efficient reuse and predictable behavior.



# ConstantBuffer.h — Templated Per-Frame Constant Buffer Wrapper

This header defines a **templated helper class** for managing DirectX 12 **constant buffers (CBVs)** with support for **multiple frames in flight**.

It encapsulates CPU-side data, GPU upload memory, descriptor heap indices, and GPU descriptor handles into a single, reusable abstraction.

---

## Purpose

`ConstantBuffer<T, FrameCount>` is designed to:

* Represent a single logical constant buffer of type `T`
* Allocate **one CBV per frame** (to avoid GPU/CPU sync hazards)
* Enforce D3D12’s **256-byte alignment requirement**
* Provide a simple CPU → GPU data update path

This is ideal for per-object, per-pass, or per-frame constant data in a modern D3D12 renderer.

---

## Template Parameters

```cpp
template<typename T, UINT FrameCount>
class ConstantBuffer
```

* `T`
  The structure defining the constant buffer layout (e.g. matrices, lighting data).

* `FrameCount`
  Number of buffered frames (commonly equal to swap chain back buffer count).

---

## Public Type Alias

```cpp
using ConstantType = T;
```

Provides a convenient way to refer to the underlying constant buffer type.

---

## CPU-Side Data

```cpp
T CpuData{};
```

* This is the **authoritative CPU copy** of the constant buffer.
* The application writes to this struct each frame.
* Data is copied into GPU memory explicitly via `CopyToGpu()`.

---

## Alignment Handling

DirectX 12 requires constant buffer views to be **256-byte aligned**.

```cpp
static constexpr UINT AlignedSize =
    (sizeof(T) + 255u) & ~255u;
```

This rounds `sizeof(T)` up to the nearest 256-byte boundary and should be used when creating CBVs and allocating upload heap memory.

---

## Per-Frame Descriptor State

Each frame gets its own descriptor and GPU handle.

```cpp
UINT HeapIndex[FrameCount]{};
CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle[FrameCount]{};
```

* `HeapIndex`
  Index into the global descriptor heap for each frame’s CBV.

* `GpuHandle`
  GPU-visible descriptor handle used for root descriptor tables.

This design avoids overwriting CBVs that the GPU may still be reading from a previous frame.

---

## GPU Resource

```cpp
Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
```

* Upload heap resource that backs the constant buffer
* Typically created once and persistently mapped
* Large enough to store `FrameCount * AlignedSize` bytes

---

## Persistently Mapped Memory

```cpp
uint8_t* MappedData = nullptr;
```

* Points to the mapped upload heap memory
* Eliminates the need for `Map` / `Unmap` every frame
* Assumes the resource remains mapped for its entire lifetime

---

## CPU → GPU Update

```cpp
void CopyToGpu(UINT frameIndex)
{
    memcpy(MappedData, &CpuData, sizeof(T));
}
```

* Copies the CPU-side struct into mapped GPU memory
* Intended to be called once per frame after updating `CpuData`
* `frameIndex` is included for API symmetry, even though the current implementation assumes per-frame offsetting is handled externally

> ⚠️ Offset management for `frameIndex` must be handled when creating the resource or computing `MappedData`.

---

## Summary

`ConstantBuffer<T, FrameCount>` provides:

* Strong typing for constant buffer data
* Automatic 256-byte alignment
* One CBV per frame to prevent GPU hazards
* Clean separation between CPU data and GPU memory
* Minimal boilerplate when updating constant buffers

This abstraction greatly simplifies constant buffer management while remaining explicit and DirectX 12–correct.



# FrameResource.cpp — Per-Frame Command List & Allocator Management

This file implements the `FrameResource` class, which encapsulates **per-frame DirectX 12 command allocators and command lists**.

The purpose of `FrameResource` is to support **multiple frames in flight** by ensuring that each frame owns its own set of command recording resources, preventing GPU/CPU synchronization hazards.

---

## High-Level Responsibilities

`FrameResource` is responsible for:

* Creating and owning multiple `ID3D12CommandAllocator` instances
* Creating matching `ID3D12GraphicsCommandList` objects
* Resetting command allocators and command lists safely per frame
* Grouping command lists into execution batches
* Cleaning up D3D12 objects on destruction

This class follows the standard **D3D12 frame resource pattern**.

---

## Constructor & Destructor

### Constructor

```cpp
FrameResource::FrameResource()
{
}
```

The constructor performs no work.
All GPU-dependent initialization is deferred to `Init()`.

---

### Destructor

```cpp
FrameResource::~FrameResource()
{
    for (int i = 0; i < COMMAND_LIST_COUNT; i++)
    {
        m_commandAllocators[i].Reset();
        m_commandAllocators[i] = nullptr;
        m_commandLists[i].Reset();
        m_commandLists[i] = nullptr;
    }
}
```

* Explicitly releases all command allocators and command lists
* Ensures COM references are dropped before shutdown
* Uses `Reset()` followed by assignment to `nullptr` for clarity and safety

---

## Initialization

### Init()

```cpp
void FrameResource::Init(DeviceResources* deviceResources)
```

This method performs all device-dependent setup for the frame resource.

#### Steps performed:

1. Retrieve the D3D12 device
2. Create one command allocator per command list
3. Create one graphics command list per allocator
4. Assign debug names
5. Close command lists immediately
6. Pre-build a batch submission array

---

### Command Allocator Creation

```cpp
ThrowIfFailed(
    d3dDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocators[i])
    )
);
```

* Uses `D3D12_COMMAND_LIST_TYPE_DIRECT`
* Each allocator corresponds 1:1 with a command list
* Required to reset allocators once the GPU has finished execution

---

### Command List Creation

```cpp
ThrowIfFailed(
    d3dDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[i].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandLists[i])
    )
);
```

* Each command list is created in an **open** state
* No initial pipeline state is bound
* Immediately closed after creation

```cpp
ThrowIfFailed(m_commandLists[i]->Close());
```

This ensures the command list is in a valid state before first use.

---

## Debug Naming

```cpp
NAME_D3D12_OBJECT_INDEXED(m_commandLists, i);
```

* Assigns readable debug names to command lists
* Extremely useful when debugging with PIX or the D3D12 debug layer

---

## Command List Batching

```cpp
BatchSubmit[0] = m_commandLists[COMMAND_LIST_SCENE_0].Get();
BatchSubmit[1] = m_commandLists[COMMAND_LIST_POST_1].Get();
```

* Prepares an array of command lists for submission
* Indicates a multi-pass rendering model (e.g. scene pass + post-processing pass)
* Allows submission via a single `ExecuteCommandLists()` call

---

## Resetting Command Lists

### ResetCommandList()

```cpp
ID3D12GraphicsCommandList* FrameResource::ResetCommandList(
    const int commandList,
    ID3D12PipelineState* pInitialState
)
```

This method prepares a command list for recording.

#### What it does:

1. Resets the associated command allocator
2. Resets the command list
3. Optionally binds an initial pipeline state
4. Returns a raw pointer for immediate use

```cpp
ThrowIfFailed(m_commandAllocators[commandList]->Reset());
ThrowIfFailed(
    m_commandLists[commandList]->Reset(
        m_commandAllocators[commandList].Get(),
        pInitialState
    )
);
```

This function must only be called **after the GPU has finished using the previous frame’s commands**.

---

## Why This Pattern Matters

DirectX 12 requires:

* Command allocators to outlive GPU execution
* Command lists to be reset only when safe
* Separate per-frame resources to avoid stalls

`FrameResource` enforces these rules by:

* Owning allocators per frame
* Centralizing command list reset logic
* Making frame lifetimes explicit

---

## Summary

`FrameResource.cpp` implements a clean and correct **per-frame command recording system** for DirectX 12:

* Multiple command lists per frame
* One allocator per command list
* Safe reset and reuse
* Built-in batching for multi-pass rendering
* Debug-friendly object naming

This class is a foundational building block for a scalable, multi-frame D3D12 renderer.


# TestFullscreen.cpp — Fullscreen-Style Quad Rendering Test

This file implements `TestFullscreen`, a simple DirectX 12 test scene that renders a **thin animated quad** across the screen using an **orthographic projection**.

It serves as a compact example of:

* Per-frame constant buffer updates
* Descriptor heap usage
* Root signatures and pipeline state setup
* Vertex buffer creation via upload + default heaps
* Integration with `FrameResource` and `GraphicsContexts`

---

## High-Level Overview

`TestFullscreen` renders a vertically oriented rectangle that smoothly moves across the screen.
The rendering pipeline uses:

* A **vertex + pixel shader pair**
* A **single constant buffer** updated every frame
* A **triangle strip quad**
* A **shader-visible CBV descriptor heap**

This class is structured like a real engine feature rather than a minimal sample.

---

## Static Configuration

```cpp
const float TestFullscreen::QuadWidth  = 20.0f;
const float TestFullscreen::QuadHeight = 720.0f;
const float TestFullscreen::ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
```

* Defines the quad’s dimensions in world space
* Uses a fixed clear color for visual contrast
* Quad height matches screen height for a “fullscreen slice” effect

---

## Per-Frame Update Logic

### Update()

```cpp
void TestFullscreen::Update(DX::StepTimer const& timer)
```

Responsibilities:

* Animates the quad horizontally
* Builds an orthographic projection matrix
* Writes per-frame constant buffer data

---

### Animation Logic

```cpp
m_sceneConstantBufferData.offset.x += translationSpeed;
if (m_sceneConstantBufferData.offset.x > offsetBounds)
{
    m_sceneConstantBufferData.offset.x = -offsetBounds;
}
```

* Moves the quad slowly along the X axis
* Wraps around once it exits the visible range

---

### Transform Construction

```cpp
XMMATRIX transform = XMMatrixMultiply(
    XMMatrixOrthographicLH(
        screenWidth,
        screenHeight,
        0.0f,
        100.0f
    ),
    XMMatrixTranslation(offset.x, 0.0f, 0.0f)
);
```

* Uses an **orthographic projection**
* Applies a translation to animate the quad
* Transposes before upload (HLSL expects column-major matrices)

---

### Constant Buffer Update

```cpp
UINT offset = frameIndex * c_alignedSceneConstantBuffer;
memcpy(mappedData + offset, &m_sceneConstantBufferData, sizeof(...));
```

* One constant buffer slice per frame
* Prevents overwriting data still in use by the GPU
* Matches the `FrameResource` multi-buffering model

---

## Rendering

### Render()

```cpp
void TestFullscreen::Render()
```

Uses a **per-frame command list** retrieved from `FrameResource`.

---

### Command List Reset

```cpp
auto* cmdList =
    m_deviceResource->GetCurrentFrameResource()
        ->ResetCommandList(
            FrameResource::COMMAND_LIST_SCENE_0,
            m_scenePipelineState.Get()
        );
```

* Ensures the command allocator is safe to reuse
* Binds the correct pipeline state
* Keeps frame ownership explicit

---

### Root Signature & Descriptor Heaps

```cpp
cmdList->SetGraphicsRootSignature(m_sceneRootSignature.Get());
cmdList->SetDescriptorHeaps(1, &GraphicsContexts::c_heap);
```

* Uses a root signature with **one CBV descriptor table**
* Binds the global shader-visible descriptor heap

---

### Constant Buffer Binding

```cpp
cmdList->SetGraphicsRootDescriptorTable(
    0,
    m_gpuHandleSceneConstantBuffer[frameIndex]
);
```

* Selects the CBV for the current frame
* Descriptor indices are allocated via `GraphicsContexts`

---

### Drawing the Quad

```cpp
cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
cmdList->IASetVertexBuffers(0, 1, &m_sceneVertexBufferView);
cmdList->DrawInstanced(4, 1, 0, 0);
```

* Triangle strip minimizes vertex count
* Quad is drawn with a single instanced draw call
* PIX markers are used for GPU debugging

---

## Device-Dependent Resource Creation

### CreateDeviceDependentResources()

This method sets up **all GPU-side state**.

---

## Root Signature

```cpp
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
```

* One descriptor table
* One CBV
* Vertex shader visibility only
* Pixel shader explicitly denied access

This keeps the root signature minimal and efficient.

---

## Pipeline State Object (PSO)

Includes:

* Input layout (position + color)
* Compiled vertex and pixel shaders
* Default rasterizer & blend states
* No depth/stencil
* Back buffer render target format

```cpp
psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
psoDesc.NumRenderTargets = 1;
```

---

## Vertex Buffer Creation

Uses the standard **upload → default heap** pattern:

1. Create default heap vertex buffer
2. Create upload heap staging buffer
3. Copy vertex data
4. Issue GPU copy
5. Transition resource to `VERTEX_AND_CONSTANT_BUFFER`

```cpp
commandList->CopyBufferRegion(...);
commandList->ResourceBarrier(...);
```

---

## Constant Buffer Creation

```cpp
BufferSize = AlignedSize * BackBufferCount;
```

* Single upload buffer
* One slice per frame
* Persistently mapped for the lifetime of the app

---

### CBV Creation Per Frame

```cpp
m_heapPositionSceneConstantBuffer[n] =
    GraphicsContexts::GetAvailableHeapPosition();
```

* Descriptor indices allocated from the global heap
* One CBV per frame
* GPU handles cached for fast binding

---

## Resource Upload Execution

```cpp
commandList->Close();
queue->ExecuteCommandLists(...);
deviceResources->WaitForGpu();
```

* Executes setup commands immediately
* Ensures vertex buffer data is ready before rendering
* Simplifies resource lifetime guarantees

---

## Summary

`TestFullscreen.cpp` demonstrates a **complete, engine-style D3D12 render path**:

* Per-frame constant buffers
* Descriptor heap–based CBV binding
* Clean command list usage via `FrameResource`
* Explicit GPU resource creation and synchronization
* Orthographic rendering with animated transforms

This file is a strong foundation for:

* Fullscreen effects
* Debug overlays
* Post-processing passes
* UI or visualization layers



