# Backend Error Handling Model

This document defines the backend error handling model for the HAL.

The HAL is a thin C++ abstraction over Vulkan, Metal, and Direct3D 12. The base layer is not a validation layer and does not attempt to make invalid API usage safe. Backend error handling is therefore intentionally narrow: it reports real failures returned by the underlying graphics backend, while expected operation states remain part of normal API results.

## Project Requirements

- The HAL is a static-library C++ API.
- C++ exceptions are required and are part of the API contract.
- No-exception builds are unsupported.
- DLL/plugin ABI usage is unsupported. Throwing C++ exceptions across binary boundaries is therefore not a supported design target.
- Destructors and reference-count release paths are `noexcept` and never report backend errors.
- The base layer does not perform validation, support checking, descriptor checking, or defensive argument checking.
- Validation, support diagnostics, and invalid-usage reporting belong to the optional validation layer.
- The HAL does not use callback-based error propagation.
- Backend failures that are discovered asynchronously are reported at the next observable API boundary, if the backend exposes them there.

## Error Categories

The API distinguishes expected operation states from backend errors.

Expected operation states are represented by ordinary return values and do not throw. Examples include:

- Fence wait timeout.
- Query not ready.
- Swapchain out-of-date or suboptimal state.
- Cache miss or non-fatal cache load state.
- Any other state that is a normal part of controlling frame flow.

Backend errors are real failures reported by Vulkan, Metal, Direct3D 12, or a backend-owned subsystem. Backend errors throw.

Examples include:

- Out of host memory.
- Out of device memory.
- Device lost or device removed.
- Backend object creation failure.
- Backend resource manipulation failure.
- Queue submission failure.
- Presentation failure that is not modeled as an expected swapchain state.
- Unexpected native API failure.

Invalid usage is not a backend error in the base layer. Invalid usage is outside the base HAL contract and may result in undefined behavior unless the validation layer is enabled.

## Object Creation Contract

Factory-style API functions return valid objects or throw.

They never return null to signal failure, and they never expose partially initialized objects.

```cpp
auto device = hal::createDevice(desc);
auto buffer = device->createBuffer(bufferDesc);
auto texture = device->createTexture(textureDesc);
auto pipeline = device->createGraphicsPipeline(pipelineDesc);
```

If one of these calls returns, the returned object is valid according to the base HAL contract. If backend creation fails, the call throws and no user-visible object is created.

Internal construction code must use RAII guards or equivalent cleanup so that partially created native backend state is released before the exception escapes.

## Backend Exceptions

Backend failures are reported with a backend exception type. The exact implementation may evolve, but each backend exception should communicate at least:

- The backend that reported the failure.
- The HAL operation that observed the failure.
- A HAL-level backend error code.
- The native backend error code or diagnostic, when available.
- The fault scope that defines the required recovery action.
- A diagnostic message suitable for logging.

An example shape:

```cpp
enum class BackendFaultScope {
    NoObjectCreated,
    ObjectInvalid,
    DeviceInvalid,
};

enum class BackendErrorCode {
    Unknown,
    NativeFailure,
    OutOfHostMemory,
    OutOfDeviceMemory,
    DeviceLost,
    SurfaceLost,
};

class BackendError : public std::exception {
public:
    BackendApi backend() const noexcept;
    BackendOperation operation() const noexcept;
    BackendErrorCode code() const noexcept;
    BackendFaultScope faultScope() const noexcept;

    std::int64_t nativeCode() const noexcept;
    const char* nativeName() const noexcept;
    const char* message() const noexcept;

    const char* what() const noexcept override;
};
```

The exception reports where the error was observed, not necessarily where the underlying GPU fault originated. This matters for asynchronous GPU execution. For example, if device loss is reported while waiting for a fence, the operation should be described as fence wait observing device loss, not as the fence wait necessarily causing device loss.

## Fault Scopes

Every backend exception has a fault scope. The fault scope defines what remains usable after the exception escapes.

### `NoObjectCreated`

The requested object or allocation was not created.

Recovery rules:

- No user-visible object exists for the failed request.
- The parent object remains operational unless the exception uses a wider fault scope.
- Existing unrelated objects remain usable.
- The caller may retry, possibly after changing parameters or freeing resources, but retry success is not guaranteed.

This scope is typical for resource allocation and object creation failures.

### `ObjectInvalid`

The target object, resource, allocator, pool, descriptor container, or similar object is in an unknown state.

Recovery rules:

- The affected object must be disposed of as soon as practical.
- The affected object must not be used for further API calls other than release/destruction.
- Any further non-release operation on the affected object is undefined behavior.
- Objects that reference the affected object must either drop the reference or be disposed of as well.
- The parent device remains operational unless the exception uses `DeviceInvalid`.

This scope is typical for resource manipulation failures where the backend may have partially applied an operation.

Examples include failures while updating, binding, mapping, aliasing, or otherwise mutating backend-owned resource state.

### `DeviceInvalid`

The logical device is in an unknown state.

Recovery rules:

- The device must be destroyed and recreated before further rendering work continues.
- All child objects of the device must be considered invalid.
- Child objects remain safe to release.
- Backend-specific partial recovery is not a portable HAL guarantee.

This scope is typical for device lost, device removed, queue submission failure, fatal presentation failure, or other backend failures where portable recovery cannot be generalized across Vulkan, Metal, and Direct3D 12.

## Out-Of-Memory Policy

Out-of-memory is reported by throwing a backend exception.

OOM is not guaranteed to be locally recoverable. Releasing resources may make a later allocation succeed, but the HAL does not guarantee that it will. Applications should use capacity, budget, and residency queries where available and avoid intentional oversubscription.

OOM fault scope depends on where the OOM is observed:

- OOM during object creation normally uses `NoObjectCreated`.
- OOM during allocator, pool, or resource manipulation may use `ObjectInvalid`; the allocator, pool, or affected object should be disposed of and recreated before retrying.
- OOM that implies broader backend or device instability may use `DeviceInvalid`.

The exception fault scope is authoritative.

## Submission And Device Operations

When a backend device operation reports a real failure, the HAL does not attempt to provide a fake local recovery path.

For example, if queue submission reports a backend error, the HAL may not be able to know portably whether work was accepted, whether the queue is usable, whether previous work was affected, or whether the device is already lost. Returning an error code does not solve that ambiguity.

Therefore backend device-operation failures throw. When the failure cannot be portably isolated, the exception should use `DeviceInvalid`, and the recovery path is full device destruction and recreation.

## Destruction

Destructors and reference-count release paths must not throw.

Most native graphics object destruction APIs do not report recoverable failure states. Even where diagnostic information may be available, it must not escape through destruction. Destruction-time diagnostics may be logged or routed through debug infrastructure, but object release must remain `noexcept`.

This is especially important because HAL objects are primarily accessed through ref-counted shared pointers. Releasing the final reference must be safe during ordinary scope exit, stack unwinding, and cleanup after a backend exception.

## Validation Layer Relationship

The base HAL deliberately does not check whether descriptors are valid, whether feature support was queried correctly, whether resource states are correct, or whether object relationships are legal.

The optional validation layer is responsible for diagnostics such as:

- Invalid descriptors.
- Unsupported format or usage combinations.
- Incorrect resource state usage.
- Invalid object lifetime or ownership relationships.
- Reuse of an object after an `ObjectInvalid` backend exception.
- Continuing to use a device after a `DeviceInvalid` backend exception.

The validation layer may track poisoned objects and produce better diagnostics, but this does not change the base HAL contract.

## Recommended User Handling Pattern

Applications should catch backend exceptions at renderer, frame, or device ownership boundaries rather than around every HAL call.

```cpp
try {
    renderer.renderFrame();
} catch (const hal::BackendError& error) {
    logBackendError(error);

    switch (error.faultScope()) {
    case hal::BackendFaultScope::NoObjectCreated:
        handleCreationFailure(error);
        break;

    case hal::BackendFaultScope::ObjectInvalid:
        disposeAffectedObjects(error);
        break;

    case hal::BackendFaultScope::DeviceInvalid:
        destroyAndRecreateDevice();
        break;
    }
}
```

This model keeps ordinary rendering code uncluttered while preserving a clear recovery contract for real backend failures.
