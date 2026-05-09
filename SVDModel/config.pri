# This switch controls if SVD is built against Tensorflow.
# This is required for using DNNs (so you will need that most likely).
# Without Tensorflow, SVD is "just" a simple state&transition model.
# To enable tensorflow, uncomment line to add to DEFINES:

# DEFINES += USE_ONNXRUNTIME

# ONNX Runtime configuration
unix {
# ONNX Runtime is expected in /opt/onnxruntime
# Override this in config.pri if needed
!isEmpty(ONNXRUNTIME_DIR) {
    INCLUDEPATH += $$ONNXRUNTIME_DIR/include
    LIBS += -L$$ONNXRUNTIME_DIR/lib -lonnxruntime
} else {
    INCLUDEPATH += /opt/onnxruntime/include
    LIBS += -L/opt/onnxruntime/lib -lonnxruntime
}
}

win32 {
# ONNX Runtime on Windows
# Set ONNXRUNTIME_DIR in your environment or here
!isEmpty(ONNXRUNTIME_DIR) {
    INCLUDEPATH += $$ONNXRUNTIME_DIR/include
    LIBS += -L$$ONNXRUNTIME_DIR/lib -lonnxruntime
}
}
