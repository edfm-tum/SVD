### Architecture Brief: SVD Model Migration from TensorFlow C++ to ONNX Runtime

**1. Rationale and Migration Objectives**
The codebase has migrated its inference engine from the TensorFlow C++ API (`libtensorflow_cc.so`) to the ONNX Runtime (ORT) C++ API.

* **Dependency Decoupling:** Eliminates the reliance on third-party compiled TensorFlow binaries.
* **Cross-Platform Parity:** ORT provides official pre-compiled C++ binaries for both Linux and Windows.
* **Performance:** Maintains zero-copy memory transfers between host and GPU.

**2. Core Paradigm Shift: Memory Ownership**
* **Modern (ONNX Runtime):** The C++ application owns the memory. `TensorWrap` classes utilize `std::vector<T>` as the underlying memory store. ORT utilizes `Ort::Value::CreateTensor`, mapping directly to these pointers (zero-copy). Boolean tensors are handled via `int8_t` storage to avoid `std::vector<bool>` bit-packing issues.

**3. Threading Architecture Constraints**
* **Directive:** `Ort::SessionOptions` are configured with intra-op and inter-op thread pools. By default, both are set to 1 to prevent contention with SVD spatial worker threads. Users can tune this via `dnn.threads.intra` and `dnn.threads.inter` in the configuration.

**4. Migration TODO List**

- [x] Remove all TensorFlow-related code and `USE_TENSORFLOW` guards.
- [x] Refactor `tensorhelper.h` to use `std::vector<T>` storage.
- [x] Integrate ONNX Runtime C++ API.
    - [x] Initialize global `Ort::Env`.
    - [x] Implement `Ort::Session` initialization in `DNN::setupDNN`.
    - [x] Implement `Ort::Session::Run` in `DNN::run`.
- [x] Add check for `.onnx` file extension in `DNN::setupDNN`.
- [x] Implement model introspection to verify input/output tensor names and types.
- [x] Optimize/Verify `getTopClasses` (CPU Top-K). Descending order (best first) is now correctly enforced.
- [x] Update build system (`config.pri` and `.pro`) for Windows support.

**5. Target Refactoring Zones**

* **`SVDModel/Predictor/tensorhelper.h`** (Finalized)
* **`SVDModel/Predictor/dnn.cpp`** (Finalized with ORT integration, introspection, and threading)
* **`SVDModel/Predictor/Predictor.pro`** (Modernized)
* **`SVDModel/SVDCore/core/model.cpp`** (Updated for spdlog 1.15.0)
