---
editor_options: 
  markdown: 
    wrap: 72
---

# Installing and compiling SVD

You can either use the precompiled version of SVD, or build SVD for
yourself.

## Installing SVD

The `executable` folder contains all the required files and libraries
for Windows. There is no separate installation. To run SVD, start the
`SVDUI.exe` ([instructions](svdUI.md)).

Note: Due to file size limits of GitHub, the library `tensorflow.dll` is
zipped (`tensorflow.zip`) in the `executable` folder. Therefore
unzipping (into the same folder) is required.

## Instructions for compiling SVD

The SVD model is a stand alone modelling software written in C++ and
available under a GPL license. SVD builds on the [Qt](https://qt.io)
framework, and is best compiled and modified with the tools provided by
Qt (e.g. the QtCreator IDE).

Currently, building SVD (and TensorFlow) is available for Windows and
Linux.

SVD consists of a number of sub-projects:

-   `Predictor`: the link to TensorFlow; this part includes
    TensorFlow-headers and contains the logic for communicating with
    TensorFlow
-   `SVDCore`: The main part of the model (representation of the
    simulated area, data, ...)
-   `SVDUI`: The Qt-based user interface

To build SVD:

-   open the `SVDModel.pro` file in QtCreator
-   Build all sub-projects
-   Run the `SVDUI.exe`

### Compiling TensorFlow on Windows

The tricky part is the compilation of the `Predictor` sub-project as
this requires a local TensorFlow installation (for include files) and a
compiled version of TensorFlow in a DLL on Windows. Check the
`Predictor.pro` file which includes some links for further information.

Contact Werner Rammer
([werner.rammer\@tum.de](mailto:werner.rammer@tum.de){.email}) for help.

### Compiling TensorFlow

Compiling TensorFlow with CMake on Windows is hard. I did go through
this (see [here](https://github.com/tensorflow/tensorflow/issues/15254)
and will likely (have to) touch the issue again. See e.g. this for
current state of compiling TF on Windows:
<https://github.com/tensorflow/tensorflow/issues/77156>

A compiled and GPU-enabled version of tensorflow.dll (version 1.4) can
be found in the `executable` folder.

### Use of precompiled Tensorflow binaries in Linux

<https://github.com/ika-rwth-aachen/libtensorflow_cc>

Follow the instructions to download and install `libtensorflow_cc`.Take
care to use the GPU version!

Use TF version up to 2.x

## other installs

FreeImage - package <https://freeimage.sourceforge.io/>

Is used in SVD for loading and saving GeoTIFF files.

```         
sudo apt-get install libfreeimage-dev
```

OpenGL Used for rendering the landscape

```         
sudo apt-get install libgl-dev
sudo apt-get install libgl1-mesa-dev
```

To use GPU on Linux

Install CUDA

See e.g. <https://neptune.ai/blog/installing-tensorflow-2-gpu-guide> or
<https://www.tensorflow.org/install/pip>

```         
sudo apt install nvidia-cuda-toolkit
```

### Notes about TF version

when installing `tensorflow_cc` (from
<https://github.com/ika-rwth-aachen/libtensorflow_cc/releases>): \* use
system-wide installations; (`dpkg -i ...`). Tried to install in a
separate folder, but I failed to get it running \* versions higher than
TF 2.9 require (minor) changes in the source code \*
