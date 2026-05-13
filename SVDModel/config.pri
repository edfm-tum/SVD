# This switch controls if SVD is built against ONNX Runtime.
# This is required for using DNNs (so you will need that most likely).
# Without ONNX, SVD is "just" a simple state&transition model.
DEFINES += USE_ONNXRUNTIME

# Uncomment the following line to enable CUDA support for ONNX Runtime
# DEFINES += USE_CUDA

# ONNX Runtime configuration
unix {
    # ONNX Runtime is expected in /opt/onnxruntime
    # Override this in config.pri if needed
    isEmpty(ONNXRUNTIME_DIR): ONNXRUNTIME_DIR = /opt/onnxruntime
    
    INCLUDEPATH += $$ONNXRUNTIME_DIR/include
    LIBONNXRUNTIME = -L$$ONNXRUNTIME_DIR/lib -lonnxruntime
}

win32 {
    # ONNX Runtime on Windows
    # Set ONNXRUNTIME_DIR in your environment or here
    isEmpty(ONNXRUNTIME_DIR): ONNXRUNTIME_DIR = C:/dev/onnxruntime
    
    INCLUDEPATH += $$ONNXRUNTIME_DIR/include
    # For MSVC, the library is usually onnxruntime.lib
    CONFIG(debug, debug|release) {
        LIBONNXRUNTIME = -L$$ONNXRUNTIME_DIR/lib -lonnxruntime
    } else {
        LIBONNXRUNTIME = -L$$ONNXRUNTIME_DIR/lib -lonnxruntime
    }
}
