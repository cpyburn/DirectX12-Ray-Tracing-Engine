# TestTriangle – DirectX 12 DXR Minimal Raytracing Sample

This is just a modified version of my DXR [tutorials](https://github.com/cpyburn/DirectX12-DXR-RTX-Tutorials-2) up to tutorial 6.  A lot of the code changed, but I intentionaly left the member and method names the same for now so that you can follow along if you did the tutorials or plan on doing those to understand this code.  Later on down the road I will change the class member and method names to more modern names.

One thing to note is I do not handle the resolution changes in this tutorial. We will visit that during the next tutorial when we do refitting.

This module implements a minimal **DirectX 12 Raytracing (DXR)** pipeline that renders a single triangle using:

- Bottom-Level Acceleration Structure (BLAS)
- Top-Level Acceleration Structure (TLAS)
- Raytracing pipeline state object
- Shader table
- UAV output texture
- `DispatchRays()` execution

It serves as a compact reference implementation of a working DXR rendering path.

---

# Overview

The rendering flow:

```

CreateDeviceDependentResources()
├── createAccelerationStructures()
├── createRtPipelineState()
├── createShaderResources()
└── createShaderTable()

Render()
└── DispatchRays()

````

---

# 1. Acceleration Structures

## Bottom-Level AS (BLAS)

Defines triangle geometry.

### Triangle vertices

```cpp
const XMFLOAT3 vertices[] =
{
    XMFLOAT3(0,      1,    0),
    XMFLOAT3(0.866f, -0.5f, 0),
    XMFLOAT3(-0.866f,-0.5f, 0),
};
````

### BLAS creation steps

1. Create vertex buffer (upload heap).
2. Fill `D3D12_RAYTRACING_GEOMETRY_DESC`
3. Query size requirements with:

   ```cpp
   GetRaytracingAccelerationStructurePrebuildInfo()
   ```
4. Create:

   * Scratch buffer (UAV)
   * Result buffer (RAYTRACING_ACCELERATION_STRUCTURE)
5. Build:

```cpp
commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
```

6. Insert UAV barrier.
7. Execute and wait for GPU.

---

## Top-Level AS (TLAS)

Defines scene instances (only one instance here).

### Instance description

```cpp
pInstanceDesc->InstanceID = 0;
pInstanceDesc->InstanceContributionToHitGroupIndex = 0;
pInstanceDesc->AccelerationStructure = mpBottomLevelAS->GetGPUVirtualAddress();
pInstanceDesc->InstanceMask = 0xFF;
```

Identity transform is used.

TLAS build follows same pattern:

* Prebuild info
* Scratch + result buffers
* `BuildRaytracingAccelerationStructure()`
* UAV barrier

---

# 2. Raytracing Pipeline State

Constructed via `D3D12_STATE_OBJECT_DESC` with 10 subobjects.

## Included Subobjects

| #  | Subobject                     |
| -- | ----------------------------- |
| 1  | DXIL Library                  |
| 2  | Hit Group                     |
| 3  | RayGen local root signature   |
| 4  | RayGen association            |
| 5  | Miss/Hit local root signature |
| 6  | Miss/Hit association          |
| 7  | Shader config                 |
| 8  | Shader config association     |
| 9  | Pipeline config               |
| 10 | Global root signature         |

---

## DXIL Library

Compiled from:

```
04-Shaders.hlsl
```

Using:

```cpp
-T lib_6_3
```

Exports:

```cpp
const WCHAR* entryPoints[] = 
{ 
    L"rayGen", 
    L"miss", 
    L"chs" 
};
```

---

## Hit Group

Associates closest-hit shader:

```cpp
hitGroupDesc.ClosestHitShaderImport = kClosestHitShader;
hitGroupDesc.HitGroupExport = kHitGroup;
```

---

## Root Signatures

### RayGen Local Root Signature

Contains descriptor table with:

* UAV (output texture)
* SRV (TLAS)

```cpp
range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
```

Associated only with:

```
rayGen
```

---

### Miss + ClosestHit Root Signature

Empty local root signature.

---

### Global Root Signature

Empty but required by DXR.

---

## Shader Config

```cpp
shaderConfig.MaxPayloadSizeInBytes = sizeof(XMFLOAT3);
shaderConfig.MaxAttributeSizeInBytes = sizeof(XMFLOAT2);
```

* Payload = RGB color
* Attributes = barycentrics

---

## Pipeline Config

```cpp
config.MaxTraceRecursionDepth = 1;
```

No secondary rays.

---

# 3. Shader Table

Contains 3 entries:

| Index | Entry    |
| ----- | -------- |
| 0     | RayGen   |
| 1     | Miss     |
| 2     | HitGroup |

---

## Entry Layout

Each entry:

```
[ShaderIdentifier][Optional Local Root Data]
```

RayGen entry includes descriptor table pointer (8 bytes).

### Entry size calculation

```cpp
mShaderTableEntrySize =
    align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT,
             D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 8);
```

---

## Writing entries

### RayGen

```cpp
memcpy(pData,
       pRtsoProps->GetShaderIdentifier(kRayGenShader),
       D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

*(UINT64*)(pData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = gpuHandle.ptr;
```

### Miss

```cpp
memcpy(pData + mShaderTableEntrySize,
       pRtsoProps->GetShaderIdentifier(kMissShader),
       D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
```

### Hit

```cpp
memcpy(pHitEntry,
       pRtsoProps->GetShaderIdentifier(kHitGroup),
       D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
```

---

# 4. Shader Resources

## Output Texture

Created as UAV:

```cpp
DXGI_FORMAT_B8G8R8A8_UNORM
```

Reason:

> sRGB formats cannot be used with UAVs.

---

## Descriptor Heap Layout

| Slot | Resource           |
| ---- | ------------------ |
| N    | UAV output texture |
| N+1  | TLAS SRV           |

TLAS SRV uses:

```cpp
D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE
```

---

# 5. Rendering

Inside `Render()`:

---

## Resource Transitions

```cpp
Transition(mpOutputResource,
           COPY_SOURCE → UNORDERED_ACCESS)
```

---

## DispatchRays Setup

```cpp
D3D12_DISPATCH_RAYS_DESC raytraceDesc;
```

### RayGen record

```cpp
raytraceDesc.RayGenerationShaderRecord.StartAddress =
    mpShaderTable->GetGPUVirtualAddress();
```

### Miss table

```cpp
raytraceDesc.MissShaderTable.StartAddress =
    base + mShaderTableEntrySize;
```

### Hit table

```cpp
raytraceDesc.HitGroupTable.StartAddress =
    base + 2 * mShaderTableEntrySize;
```

---

## Bind and Dispatch

```cpp
m_sceneCommandList->SetComputeRootSignature(mpEmptyRootSig.Get());
m_sceneCommandList->SetPipelineState1(mpPipelineState.Get());
m_sceneCommandList->DispatchRays(&raytraceDesc);
```

---

## Copy to Back Buffer

1. Transition output to `COPY_SOURCE`
2. Transition intermediate render target to `COPY_DEST`
3. Copy
4. Restore states

---

# 6. DXC Shader Compilation

DXR library compiled via:

```cpp
-T lib_6_3
-HV 2021
-Zi
-Qembed_debug
-Od
-WX
```

Compilation flow:

```cpp
compiler->Compile(...)
result->GetOutput(DXC_OUT_OBJECT, ...)
```

Errors are printed via `OutputDebugStringA`.

---

# Class Lifecycle

## Initialization

```cpp
CreateDeviceDependentResources()
```

Creates:

* BLAS
* TLAS
* Pipeline
* Resources
* Shader table

---

## Rendering

```cpp
Render()
```

Executes `DispatchRays()` and copies result to swap chain.

---

## Cleanup

```cpp
Release()
```

Releases:

* Vertex buffer
* BLAS
* TLAS
* Pipeline state
* Root signatures
* Shader table
* Output resource

---

# Key DXR Concepts Demonstrated

* BLAS / TLAS construction
* Instance descriptors
* DXIL shader libraries
* Hit groups
* Local vs global root signatures
* Shader table layout
* UAV output textures
* `DispatchRays()`
* Resource state transitions
* Descriptor heap binding

---

# Summary

This file implements a **minimal but complete DirectX 12 DXR pipeline** that:

* Builds acceleration structures for a single triangle
* Compiles and links raytracing shaders
* Creates a raytracing pipeline state object
* Builds a shader table
* Dispatches rays
* Copies the raytraced result to the swap chain

It is a clean reference for understanding how the full DXR pipeline fits together at the API level.

```
```

