# CK2 - Virtools Behavioral Engine

CK2 is a source-level reimplementation of the Virtools behavioral engine used by Ballance. It provides the object model, behavioral scripting runtime, managers, scenes, parameter system, and file serialization expected by the surrounding Ballanced runtime.

## Support scope

The instructions in this document describe CK2's `sdl` branch. That branch is continuously built as part of [Ballanced](https://github.com/doyaGu/Ballanced) on Windows, Linux, and macOS. The Ballanced root `CMakePresets.json` is the source of truth for the supported full-runtime platform and architecture matrix.

Standalone CK2 builds use this repository's CMake options and can have a narrower dependency/toolchain matrix than the assembled Ballanced runtime.

## Features

- Virtools-compatible object hierarchy, IDs, class registration, and serialization
- Visual behavior graphs, links, parameters, messages, and scheduling
- Scene, level, group, render, path, time, and plugin management
- CMO/NMO-facing state chunks and file-loading infrastructure
- Cross-platform math and platform services through VxMath

## Architecture

- `CKContext`: central engine context and manager owner
- `CKObject`: base object, identity, lifetime, and serialization
- `CKBeObject`: behavior-capable objects and message handling
- `CKBehavior`: visual scripting graphs and execution
- `CK3dEntity`: transform hierarchy for 3D objects
- Managers: object, parameter, behavior, message, time, path, plugin, and related subsystems

## Building

### Recommended: Ballanced superproject

For a runnable game runtime, build CK2 through Ballanced so the exact VxMath, Player, renderer, managers, plugins, and Building Blocks commits are used together:

```bash
git clone --recurse-submodules https://github.com/doyaGu/Ballanced.git
cd Ballanced
cmake --preset macos-arm64-tests # choose the preset for your host
cmake --build --preset macos-arm64-tests-release
ctest --preset macos-arm64-tests-release
```

See the [Ballanced build guide](https://github.com/doyaGu/Ballanced/blob/sdl/BUILD.md) for the complete preset matrix.

### Standalone

Requirements:

- CMake 3.16+
- A C++ toolchain supported by CMake
- The nested `miniz` submodule
- VxMath, resolved from an adjacent `../VxMath` checkout when present or fetched by the standalone build
- Network access when CMake needs to fetch VxMath or GoogleTest

```bash
git clone --branch sdl --recurse-submodules https://github.com/doyaGu/CK2.git
cd CK2
cmake -S . -B build -DCK2_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For single-configuration generators such as Ninja, set `-DCMAKE_BUILD_TYPE=Release`; the `--config Release` and `-C Release` arguments may be omitted.

### CMake options

- `CK2_BUILD_SHARED`: build the shared library; default `ON`
- `CK2_BUILD_STATIC`: build the static library; default `OFF`
- `CK2_BUILD_TESTS`: build the regression suite; default `OFF`
- `CK2_INSTALL`: generate install rules; defaults to `ON` for a standalone build

## Testing

The GoogleTest suite covers state chunks, object and scene lifetimes, parameters, behavior links and scheduling, path handling, plugins, and file-decoder regressions.

```bash
ctest --test-dir build -C Release --output-on-failure
```

## Basic API

```cpp
#include "CKAll.h"

CKContext *context = nullptr;
if (CKCreateContext(&context, nullptr, 0) == CK_OK) {
    CKObject *object = context->CreateObject(CKCID_3DENTITY, "Entity");
    // Use the object through the CK2 API.
    CKCloseContext(context);
}
```

### Deterministic fixed-clock processing

Authoritative or headless runtimes can supply an exact unscaled delta in
milliseconds. `ProcessFixed` enables the external clock and processes one frame;
the time manager will not replace that delta with elapsed wall time. Existing
minimum/maximum delta limits and the time scale factor still apply.

```cpp
constexpr float kServerTickMilliseconds = 1000.0f / 66.0f;
context->ProcessFixed(kServerTickMilliseconds);

CKTimeManager *time = context->GetTimeManager();
time->SetExternalClock(FALSE); // Resume wall-clock Process() calls.
```

`ProcessFixed` rejects non-finite and non-positive deltas with
`CKERR_INVALIDPARAMETER` without processing a frame.

### Strict file loading

Contexts default to `CK_LOAD_POLICY_LEGACY`, preserving the historical behavior
that allows a direct `CKFile::Load` to continue after dependency checking reports
`CKERR_PLUGINSMISSING`. Servers that must authenticate every map dependency can
enable strict loading before opening any top-level or nested file:

```cpp
if (context->SetLoadPolicy(CK_LOAD_POLICY_STRICT) != CK_OK) {
    // Invalid policy value.
}

CKERROR result = context->Load("Level_01.NMO", &loadedObjects);
// Strict mode automatically adds CK_LOAD_CHECKDEPENDENCIES. Only CK_OK loads
// object data; missing plugins and every other open error are returned directly.
```

The policy also applies to `CKFile` instances created by the context, including
the staged `OpenFile`/`OpenMemory` plus `LoadFileData` API. Set
`CK_LOAD_POLICY_LEGACY` explicitly to restore legacy behavior.

## Dependencies

- VxMath for math and platform services
- miniz for compressed data
- GoogleTest for test builds

## Versioning

CK2 is versioned independently. Ballanced releases pin an exact CK2 commit; the CK2 version is not expected to match the Ballanced or BallancePlayer version.

## License

Apache License 2.0. See [LICENSE](LICENSE).
