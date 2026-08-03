#ifndef TENSORHELPER_H
#define TENSORHELPER_H

#include "inputtensoritem.h"
#include <onnxruntime_cxx_api.h>

#if defined(__has_include)
  #if __has_include(<cuda_runtime.h>)
    #include <cuda_runtime.h>
    #define HAS_CUDA_RUNTIME 1
  #endif
#endif

#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <typeinfo>
#include <cassert>
#include <memory>

// Trait to handle std::vector<bool> specialization issue (bit-packing)
// ONNX Runtime and raw data access require byte-addressable bools.
template<typename T>
struct StorageTrait {
    using Type = T;
};

template<>
struct StorageTrait<bool> {
    using Type = int8_t; // Use 1 byte for bool
};

// Portable Pinned Memory Allocator Wrapper
template<typename T>
class PinnedBuffer {
public:
    PinnedBuffer() : mData(nullptr), mSize(0), mIsPinned(false) {}

    void resize(size_t count) {
        if (mSize == count && mData != nullptr)
            return;
        freeMemory();
        mSize = count;
        if (mSize == 0)
            return;

        size_t nbytes = mSize * sizeof(T);

#if defined(HAS_CUDA_RUNTIME)
        cudaError_t err = cudaHostAlloc((void**)&mData, nbytes, cudaHostAllocDefault);
        if (err == cudaSuccess && mData != nullptr) {
            mIsPinned = true;
            return;
        }
#endif

        // Fallback to standard CPU heap memory if CUDA is unavailable or allocation failed
        mData = new T[mSize];
        mIsPinned = false;
    }

    ~PinnedBuffer() {
        freeMemory();
    }

    T* data() { return mData; }
    const T* data() const { return mData; }

private:
    void freeMemory() {
        if (mData) {
#if defined(HAS_CUDA_RUNTIME)
            if (mIsPinned) {
                cudaFreeHost(mData);
                mData = nullptr;
                return;
            }
#endif
            delete[] mData;
            mData = nullptr;
        }
    }

    T* mData;
    size_t mSize;
    bool mIsPinned;
};

class TensorWrapper {
public:
    // Expose dataType independent of Framework
    virtual InputTensorItem::DataType dataType() const = 0;

    virtual int ndim() const = 0;
    virtual std::string asString(size_t example) const = 0;
    virtual void* getRawDataPtr() = 0;
    virtual ~TensorWrapper() {}
};


template<typename T>
class TensorWrap1d : public TensorWrapper
{
public:
    using InternalType = typename StorageTrait<T>::Type;

    TensorWrap1d() {
        // Determine DataType
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;
        
        mData.resize(1);
    }
    ~TensorWrap1d() {}

    InputTensorItem::DataType dataType() const override { return mDataType; }

    T value() const { return static_cast<T>(mData.data()[0]); }
    void setValue(T value) { mData.data()[0] = static_cast<InternalType>(value); }
    
    size_t n() const  { return 1; }
    int ndim() const { return 0; }
    
    std::string asString(size_t /*example*/) const {
        std::stringstream ss;
        ss << "Scalar: " << value();
        return ss.str();
    }

    InternalType* data() { return mData.data(); }
    const InternalType* data() const { return mData.data(); }
    void* getRawDataPtr() override { return mData.data(); }

private:
    InputTensorItem::DataType mDataType;
    PinnedBuffer<InternalType> mData;
};


template<typename T>
class TensorWrap2d : public TensorWrapper
{
public:
    using InternalType = typename StorageTrait<T>::Type;

    TensorWrap2d(size_t batch_size, size_t n) {
        mBatchSize = batch_size; mN=n;
        
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;

        mData.resize(mBatchSize * mN);
    }

    InputTensorItem::DataType dataType() const override { return mDataType; }

    size_t n() const  { return mN; }
    int ndim() const { return 2; }
    size_t batchSize() const { return mBatchSize; }
    
    T *example(size_t element) {
        assert(element < mBatchSize);
        return reinterpret_cast<T*>(mData.data() + element*mN); 
    }
    const T *example(size_t element) const {
        assert(element < mBatchSize);
        return reinterpret_cast<const T*>(mData.data() + element*mN); 
    }

    std::string asString(size_t element) const {
        const T* p=example(element);
        std::stringstream ss;
        if (p) {
            for (size_t i=0;i<n();++i)
                ss << *p++ << " ";
        }
        return ss.str();
    }

    InternalType* data() { return mData.data(); }
    const InternalType* data() const { return mData.data(); }
    void* getRawDataPtr() override { return mData.data(); }

    ~TensorWrap2d() { 
    }
private:
    InputTensorItem::DataType mDataType;
    PinnedBuffer<InternalType> mData;
    size_t mBatchSize;
    size_t mN;
};

template<typename T>
class TensorWrap3d : public TensorWrapper
{
public:
    using InternalType = typename StorageTrait<T>::Type;

    TensorWrap3d(size_t batch_size, size_t nx, size_t ny) {
        mBatchSize = batch_size; mRows=nx; mCols=ny;
        
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;

        mData.resize(mBatchSize * mRows * mCols);
    }

     ~TensorWrap3d() { 
     }
    
    InputTensorItem::DataType dataType() const override { return mDataType; }

    size_t rows() const { return mRows; }
    size_t cols() const {return mCols; }
    
    T *example(size_t element) {
        assert(element < mBatchSize);
        return reinterpret_cast<T*>(mData.data() + element*mRows*mCols); 
    }
    const T *example(size_t element) const {
        assert(element < mBatchSize);
        return reinterpret_cast<const T*>(mData.data() + element*mRows*mCols); 
    }

    T *row(size_t element, size_t row) {
        assert(element < mBatchSize && row < mRows);
        return reinterpret_cast<T*>(mData.data() + element*mRows*mCols+row*mCols); 
    }
    const T *row(size_t element, size_t row) const {
        assert(element < mBatchSize && row < mRows);
        return reinterpret_cast<const T*>(mData.data() + element*mRows*mCols+row*mCols); 
    }

    int ndim() const { return 3; }
    size_t batchSize() const { return mBatchSize; }
    
    std::string asString(size_t example) const {
        std::stringstream ss;
        for (size_t r=0;r<rows(); ++r) {
            const T* row_ptr = row(example, r);
            if (row_ptr) {
                for (size_t c=0;c<cols(); ++c)
                    ss << row_ptr[c] << " ";
            }
            ss << std::endl;
        }
        return ss.str();
    }

    InternalType* data() { return mData.data(); }
    const InternalType* data() const { return mData.data(); }
    void* getRawDataPtr() override { return mData.data(); }


private:
    InputTensorItem::DataType mDataType;
    PinnedBuffer<InternalType> mData;
    size_t mBatchSize;
    size_t mRows;
    size_t mCols;
};

#endif // TENSORHELPER_H
