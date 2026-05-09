### Architecture Brief: SVD Model Migration from TensorFlow C++ to ONNX Runtime

**1. Rationale and Migration Objectives**
The codebase is migrating its inference engine from the TensorFlow C++ API (`libtensorflow_cc.so`) to the ONNX Runtime (ORT) C++ API.

* **Dependency Decoupling:** Eliminates the reliance on third-party compiled TensorFlow binaries.
* **Cross-Platform Parity:** ORT provides official pre-compiled C++ binaries for both Linux and Windows.
* **Performance:** Maintains zero-copy memory transfers between host and GPU.

**2. Core Paradigm Shift: Memory Ownership**
* **Modern (ONNX Runtime):** The C++ application owns the memory. `TensorWrap` classes now utilize `std::vector<T>` as the underlying memory store. ORT will utilize `Ort::Value::CreateTensor`, wrapping these pointers.

**3. Threading Architecture Constraints**
* **Directive:** When initializing `Ort::SessionOptions` in `dnn.cpp`, the intra-op and inter-op thread pools must be strictly limited to 1 to prevent contention with SVD worker threads.

**4. Migration TODO List**

- [x] Remove all TensorFlow-related code and `USE_TENSORFLOW` guards.
- [x] Refactor `tensorhelper.h` to use `std::vector<T>` storage.
- [ ] Integrate ONNX Runtime C++ API.
    - [ ] Initialize global `Ort::Env`.
    - [ ] Implement `Ort::Session` initialization in `DNN::setupDNN`.
    - [ ] Implement `Ort::Session::Run` in `DNN::run`.
- [ ] Add check for `.onnx` file extension in `DNN::setupDNN`. (Partially done)
- [ ] Implement model introspection to verify input/output tensor names and types.
- [ ] Optimize/Verify `getTopClasses` (CPU Top-K).
- [ ] Update build system (`config.pri` and `.pro`) for Windows support.

**5. Target Refactoring Zones**

* **`SVDModel/Predictor/tensorhelper.h`** (Refactored)
* **`SVDModel/Predictor/dnn.cpp`** (Stripped, awaiting ORT integration)
* **`SVDModel/Predictor/Predictor.pro`** (Cleaned up)
