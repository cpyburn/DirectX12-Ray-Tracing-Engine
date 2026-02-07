This tutorial is going to add fullscreen capability to the library. For the most part, we are backwards engineering / copy pasteing this [Microsoft DirectX 12 sample project](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Fullscreen) and adding it to ours. You may ask why? It is a good way for us to test fullscreen throughout the building of the engine and I personally think all games should handle resizing of the screen correctly.

The main things to take away from here are the addition and refactoring of the following classes.

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


Absolutely — here’s the **updated `README.md` content** reflecting the refactor to your new `ConstantBuffer<T, FrameCount>` abstraction and the cleaned-up per-frame update path.

You can copy-paste this directly into GitHub.

---

# TestFullscreen.cpp — Animated Fullscreen Quad Using Templated Constant Buffers

This file implements `TestFullscreen`, a DirectX 12 test scene that renders a **thin animated quad** across the screen using an **orthographic projection**.

This version has been refactored to use the engine’s templated
`ConstantBuffer<T, FrameCount>` helper, greatly simplifying constant buffer
management and per-frame updates.

---

## High-Level Overview

`TestFullscreen` demonstrates a complete render path built on engine
infrastructure:

* Per-frame command lists via `FrameResource`
* Descriptor allocation via `GraphicsContexts`
* A **templated constant buffer** with one CBV per frame
* A minimal root signature and graphics PSO
* Upload → default heap resource initialization

The quad continuously scrolls across the screen, making this a useful visual
sanity check and foundation for fullscreen or overlay-style rendering.

---

## Static Configuration

```cpp
const float QuadWidth  = 20.0f;
const float QuadHeight = 720.0f;
const float ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
```

* The quad is a tall, thin rectangle
* Height roughly matches screen height
* A fixed clear color provides contrast

---

## Per-Frame Update Logic

### Update()

```cpp
void TestFullscreen::Update(DX::StepTimer const& timer)
```

Responsibilities:

* Animate the quad’s horizontal offset
* Build an orthographic transform
* Write updated data into the per-frame constant buffer

---

### Animation

```cpp
m_sceneConstantBuffer.CpuData.offset.x += translationSpeed;
if (m_sceneConstantBuffer.CpuData.offset.x > offsetBounds)
{
    m_sceneConstantBuffer.CpuData.offset.x = -offsetBounds;
}
```

* Moves the quad slowly along the X axis
* Wraps once it exits the visible bounds

---

### Transform Construction

```cpp
XMMATRIX transform = XMMatrixMultiply(
    XMMatrixOrthographicLH(screenWidth, screenHeight, 0.0f, 100.0f),
    XMMatrixTranslation(offset.x, 0.0f, 0.0f)
);
```

* Uses an **orthographic projection**
* Applies a translation to animate the quad
* Transposed before upload for HLSL consumption

---

### GPU Upload

```cpp
m_sceneConstantBuffer.CopyToGpu(currentFrameIndex);
```

* Copies CPU data into persistently mapped upload memory
* One constant buffer slice per frame
* Eliminates manual pointer arithmetic and memcpy logic

---

## Rendering

### Render()

```cpp
void TestFullscreen::Render()
```

Rendering uses a **per-frame command list** provided by `FrameResource`.

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

* Ensures allocator safety
* Binds the correct PSO
* Keeps command ownership frame-scoped

---

### Root Signature & Descriptor Heap

```cpp
cmdList->SetGraphicsRootSignature(m_sceneRootSignature.Get());
cmdList->SetDescriptorHeaps(1, &GraphicsContexts::c_heap);
```

* Root signature contains a single CBV descriptor table
* Uses the global shader-visible descriptor heap

---

### Constant Buffer Binding

```cpp
cmdList->SetGraphicsRootDescriptorTable(
    0,
    m_sceneConstantBuffer.GpuHandle[currentFrameIndex]
);
```

* Selects the CBV corresponding to the active frame
* Descriptor indices are allocated via `GraphicsContexts`

---

### Drawing the Quad

```cpp
cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
cmdList->IASetVertexBuffers(0, 1, &m_sceneVertexBufferView);
cmdList->DrawInstanced(4, 1, 0, 0);
```

* Uses a triangle strip to minimize vertex count
* Single draw call
* PIX markers are included for GPU debugging

---

## Device-Dependent Resource Creation

### CreateDeviceDependentResources()

This method initializes all GPU objects.

---

## Root Signature

* One descriptor table
* One CBV
* Vertex shader visibility only
* Pixel shader access explicitly denied

This keeps the root signature minimal and efficient.

---

## Pipeline State Object (PSO)

Includes:

* Vertex + pixel shaders from `sceneShaders.hlsl`
* Position + color input layout
* Default rasterizer and blend states
* No depth/stencil
* Back buffer render target format

---

## Vertex Buffer Creation

Uses the standard **upload → default heap** pattern:

1. Create default heap vertex buffer
2. Create upload heap staging buffer
3. Copy CPU data into upload buffer
4. Issue GPU copy
5. Transition to `VERTEX_AND_CONSTANT_BUFFER`

---

## Constant Buffer Creation (Refactored)

```cpp
ConstantBuffer<SceneConstants, FrameCount> m_sceneConstantBuffer;
```

Key characteristics:

* Single upload heap resource
* 256-byte aligned per frame
* One CBV per back buffer
* Persistently mapped for lifetime

---

### CBV Allocation

```cpp
m_sceneConstantBuffer.HeapIndex[n] =
    GraphicsContexts::GetAvailableHeapPosition();
```

* Descriptor indices allocated from the global heap
* GPU handles cached per frame
* Matches `FrameResource` buffering model

---

### Persistent Mapping

```cpp
m_sceneConstantBuffer.Resource->Map(
    0, nullptr,
    reinterpret_cast<void**>(&m_sceneConstantBuffer.MappedData)
);
```

* Upload buffer remains mapped for the lifetime of the app
* Safe and recommended for constant buffers
* Simplifies per-frame updates

---

## Resource Upload Execution

```cpp
commandList->Close();
queue->ExecuteCommandLists(...);
deviceResources->WaitForGpu();
```

* Executes initialization commands immediately
* Guarantees vertex data and CBVs are ready before rendering

---

## Summary

This updated version of `TestFullscreen.cpp` demonstrates:

* Clean per-frame animation logic
* A modern, reusable constant buffer abstraction
* Descriptor heap–based CBV binding
* Frame-safe command list management
* Minimal but production-correct D3D12 patterns

This file now serves as a strong reference for:

* Fullscreen passes
* Debug overlays
* UI rendering
* Post-processing groundwork
