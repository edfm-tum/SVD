# Installing and compiling SVD

You can either use the precompiled version of SVD, or build SVD for yourself.

## Installing SVD

The `executable` folder contains all the required files and libraries for Windows. There is no separate installation. To run SVD, start the `SVDUI.exe` ([instructions](svdUI.md)).

For DNN inference, SVD now uses **ONNX Runtime**. The required library `onnxruntime.dll` (CPU version) is included in the `executable` folder.

## Instructions for compiling SVD

The SVD model is a stand alone modelling software written in C++ and available under a GPL license. SVD builds on the [Qt](https://qt.io) framework, and is best compiled and modified with the tools provided by Qt (e.g. the QtCreator IDE).

Currently, building SVD is available for Windows, Linux, and macOS.

SVD consists of a number of sub-projects:

-   `Predictor`: the link to ONNX Runtime; this part includes ONNX-headers and contains the logic for communicating with the inference engine.
-   `SVDCore`: The main part of the model (representation of the simulated area, data, ...)
-   `SVDUI`: The Qt-based user interface

To build SVD:

-   open the `SVDModel.pro` file in QtCreator
-   Build all sub-projects
-   Run the `SVDUI.exe` (Windows) or the resulting binary (Linux/Mac)

### Compiling SVD with ONNX Runtime

SVD requires the ONNX Runtime C++ API. 

> [!IMPORTANT]
> **Do NOT use `pip install`, `nuget`, or other package managers** for installing ONNX Runtime for SVD. These are intended for Python or .NET environments. For SVD (C++), you must download the precompiled **shared library** binaries (headers and `.so` / `.dll` files) directly from the [Official GitHub Releases](https://github.com/microsoft/onnxruntime/releases).

#### Windows

1.  Download the ONNX Runtime binaries (e.g., `onnxruntime-win-x64-n.n.n.zip`) from the [official releases](https://github.com/microsoft/onnxruntime/releases).
2.  Extract the archive to a local directory (e.g., `C:\dev\onnxruntime`).
3.  In SVD's `SVDModel/config.pri`, set the `ONNXRUNTIME_DIR` to this path if it differs from the default.
4.  Copy `onnxruntime.dll` to your build output folder (where `SVDUI.exe` resides).

#### Linux (Ubuntu / Fedora)

The build system expects ONNX Runtime to be located in `/opt/onnxruntime`.

**Example: Manual installation for GPU support**

```bash
# 1. Choose a version compatible with your CUDA/cuDNN (e.g., 1.17.1)
VERSION="1.17.1"
FILENAME="onnxruntime-linux-x64-gpu-${VERSION}.tgz"

# 2. Download from GitHub
wget https://github.com/microsoft/onnxruntime/releases/download/v${VERSION}/${FILENAME}

# 3. Extract and move to /opt/onnxruntime
sudo mkdir -p /opt/onnxruntime
sudo tar -xzf ${FILENAME} -C /opt/onnxruntime --strip-components=1

# 4. Cleanup
rm ${FILENAME}

# 5. Configure the dynamic linker so the OS can find the libraries
echo "/opt/onnxruntime/lib" | sudo tee /etc/ld.so.conf.d/onnxruntime.conf
sudo ldconfig
```

If you only need CPU support, replace `gpu` with `cpu` in the filename above.

#### macOS

1.  Download the macOS binaries (universal or x64/arm64 depending on your hardware).
2.  Extract to a preferred location and update `ONNXRUNTIME_DIR` in `SVDModel/config.pri`.
3.  Alternatively, you can install via Homebrew (`brew install onnxruntime`), but ensure the paths in `config.pri` match (Homebrew usually installs to `/usr/local` on Intel or `/opt/homebrew` on Apple Silicon).

### Using GPU acceleration (CUDA)

To use NVIDIA GPUs for faster DNN inference:

1.  **Requirement:** You must have an NVIDIA GPU and compatible drivers installed.
2.  **ONNX Runtime:** Download the **GPU-enabled** version of ONNX Runtime (e.g., `onnxruntime-linux-x64-gpu-*`).
3.  **CUDA & cuDNN:** Install the versions of CUDA and cuDNN that are compatible with the ONNX Runtime version you downloaded (check the [ONNX Runtime documentation](https://onnxruntime.ai/docs/execution-providers/CUDA-ExecutionProvider.html#requirements)).
4.  **Build Configuration:** In `SVDModel/config.pri`, uncomment the line:
    `DEFINES += USE_CUDA`
5.  **Execution:** When SVD starts, it will attempt to initialize the CUDA execution provider. Check the log output to verify if it succeeded or fell back to CPU.

## other installs

### FreeImage

Used in SVD for loading and saving GeoTIFF files.

**Linux:**
```bash
sudo apt-get install libfreeimage-dev   # Ubuntu
sudo dnf install FreeImage-devel        # Fedora
```

### OpenGL

Used for rendering the landscape.

**Linux:**
```bash
sudo apt-get install libgl-dev libgl1-mesa-dev  # Ubuntu
sudo dnf install mesa-libGL-devel               # Fedora
```

## SVD without ONNX

SVD can be used *without* ONNX Runtime. This version is not able to use DNNs for estimating state transitions, but can still be useful, e.g., as a pure state-and-transition-model using the [matrix module](module_matrix.md).

To build SVD without ONNX, you need to update the file `SVDModel/config.pri`. To *disable* ONNX, comment out this line:

`DEFINES += USE_ONNXRUNTIME`

Save and recompile (ensure that `qmake` is executed).
