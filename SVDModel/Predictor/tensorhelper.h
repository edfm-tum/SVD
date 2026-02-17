#ifndef TENSORHELPER_H
#define TENSORHELPER_H

#ifdef USE_TENSORFLOW
#ifdef COMPILER_MSVC
#pragma warning(push, 0)
#endif
#include "tensorflow/core/framework/tensor.h"
#ifdef COMPILER_MSVC
#pragma warning(pop)
#endif
#endif

#include "inputtensoritem.h"

#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <typeinfo>
#include <cassert>

#ifdef USE_TENSORFLOW
/// Some array conversion tools.
/// \author David Stutz
template<typename T, int NDIMS>
class TensorConversion {
public:

  /// Access the underlying data pointer of the tensor.
  /// \param tensor
  /// \return
  static T* AccessDataPointer(const tensorflow::Tensor &tensor) {
    // get underlying Eigen tensor
    auto tensor_map = tensor.tensor<T, NDIMS>();
    // get the underlying array
    auto array = tensor_map.data();
    return const_cast<T*>(array);
  }
};
#endif


class TensorWrapper {
public:
#ifdef USE_TENSORFLOW
    virtual tensorflow::Tensor &tensor() = 0;
#endif
    // Expose dataType independent of TensorFlow
    virtual InputTensorItem::DataType dataType() const = 0;

    virtual int ndim() const = 0;
    virtual std::string asString(size_t example) const = 0;
    virtual ~TensorWrapper() {}
};


template<typename T>
class TensorWrap1d : public TensorWrapper
{
public:
    TensorWrap1d() {
        // Determine DataType (independent of TF)
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;

#ifdef USE_TENSORFLOW
        tensorflow::DataType dt = static_cast<tensorflow::DataType>(mDataType);
        // create a scalar
        mTensor = tensorflow::Tensor(dt, tensorflow::TensorShape());
#endif
    }
    ~TensorWrap1d() {}

    InputTensorItem::DataType dataType() const override { return mDataType; }

#ifdef USE_TENSORFLOW
    tensorflow::Tensor &tensor() override { return mTensor; }
    
    T value() const { return mTensor.scalar<bool>()(); }
    void setValue(T value) { mTensor.scalar<bool>()() = value; }
#else
    T value() const { return T(); }
    void setValue(T value) { }
#endif
    size_t n() const  { return 1; }
    int ndim() const { return 0; }
    
    std::string asString(size_t /*example*/) const {
        std::stringstream ss;
        ss << "Scalar: " << value();
        return ss.str();
    }
private:
    InputTensorItem::DataType mDataType;
#ifdef USE_TENSORFLOW
    tensorflow::Tensor mTensor;
#endif
};


template<typename T>
class TensorWrap2d : public TensorWrapper
{
public:
    TensorWrap2d(size_t batch_size, size_t n) {
        mBatchSize = batch_size; mN=n;
        
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;

#ifdef USE_TENSORFLOW
        tensorflow::DataType dt = static_cast<tensorflow::DataType>(mDataType);
        mT = new tensorflow::Tensor(dt, tensorflow::TensorShape({ static_cast<int>(mBatchSize), static_cast<int>(mN)}));
        mData = TensorConversion<T,2>::AccessDataPointer(*mT);
        mPrivateTensor=true;
#else
        mData = nullptr; 
#endif
        mNBytes = sizeof(T) * mBatchSize * mN;
    }
#ifdef USE_TENSORFLOW
    TensorWrap2d(tensorflow::Tensor &tensor) {
        mBatchSize = tensor.dim_size(0);
        mN = tensor.dim_size(1);
        mT = &tensor;
        mData = TensorConversion<T,2>::AccessDataPointer(tensor);
        mPrivateTensor=false;
        mNBytes = sizeof(T) * mBatchSize * mN;
        // Guess datatype from T? Or from tensor?
        // We can't easily get InputTensorItem::DataType from tensorflow::Tensor here without a map.
        // But since this constructor is only used when TF is available, maybe we don't strictly need mDataType to be accurate for this specific constructor if it's not used in a way that requires it?
        // However, let's try to set it.
        // For now, we trust T.
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;
    }
    tensorflow::Tensor &tensor() override { return *mT; }
#endif
    InputTensorItem::DataType dataType() const override { return mDataType; }

    size_t n() const  { return mN; }
    int ndim() const { return 2; }
    size_t batchSize() const { return mBatchSize; }
    
    T *example(size_t element) const {
#ifdef USE_TENSORFLOW
        assert(element*mN*sizeof(T)<mNBytes);
        return mData + element*mN; 
#else
        return nullptr;
#endif
    }
    std::string asString(size_t element) const {
        T* p=example(element);
        std::stringstream ss;
        if (p) {
            for (size_t i=0;i<n();++i)
                ss << *p++ << " ";
        }
        return ss.str();
    }

    ~TensorWrap2d() { 
#ifdef USE_TENSORFLOW
        if (mPrivateTensor) delete mT; 
#endif
    }
private:
    InputTensorItem::DataType mDataType;
#ifdef USE_TENSORFLOW
    tensorflow::Tensor *mT;
    bool mPrivateTensor;
#endif
    T *mData;
    size_t mBatchSize;
    size_t mN;
    size_t mNBytes;
};

template<typename T>
class TensorWrap3d : public TensorWrapper
{
public:
    TensorWrap3d(size_t batch_size, size_t nx, size_t ny) {
        mBatchSize = batch_size; mRows=nx; mCols=ny;
        
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;

#ifdef USE_TENSORFLOW
        tensorflow::DataType dt = static_cast<tensorflow::DataType>(mDataType);
        mT = new tensorflow::Tensor(dt, tensorflow::TensorShape({ static_cast<int>(mBatchSize), static_cast<int>(mRows), static_cast<int>(mCols)}));
        mData = TensorConversion<T,3>::AccessDataPointer(*mT);
        mPrivateTensor = true;
#else
        mData = nullptr;
#endif
        mNBytes = sizeof(T) * mBatchSize * mRows * mCols;
    }
#ifdef USE_TENSORFLOW
    TensorWrap3d(tensorflow::Tensor &tensor) {
        mBatchSize = tensor.dim_size(0);
        mRows = tensor.dim_size(1);
        mCols = tensor.dim_size(2);
        mT = &tensor;
        mData = TensorConversion<T,3>::AccessDataPointer(tensor);
        mPrivateTensor=false;
        mNBytes = sizeof(T) * mBatchSize * mRows * mCols;
        
        mDataType = InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(float)) mDataType=InputTensorItem::DT_FLOAT;
        if (typeid(T)==typeid(int64_t)) mDataType=InputTensorItem::DT_INT64;
        if (typeid(T)==typeid(int32_t)) mDataType=InputTensorItem::DT_INT32;
        if (typeid(T)==typeid(unsigned short)) mDataType=InputTensorItem::DT_UINT16;
        if (typeid(T)==typeid(short int)) mDataType=InputTensorItem::DT_INT16;
        if (typeid(T)==typeid(bool)) mDataType=InputTensorItem::DT_BOOL;
    }
    tensorflow::Tensor &tensor() override { return *mT; }
#endif

     ~TensorWrap3d() { 
#ifdef USE_TENSORFLOW
         if (mPrivateTensor) delete mT; 
#endif
     }
    
    InputTensorItem::DataType dataType() const override { return mDataType; }

    size_t rows() const { return mRows; }
    size_t cols() const {return mCols; }
    T *example(size_t element) {
#ifdef USE_TENSORFLOW
        assert(element*mRows*mCols*sizeof(T)<mNBytes);
        return mData + element*mRows*mCols; 
#else
        return nullptr;
#endif
    }
    T *row(size_t element, size_t row) const {
#ifdef USE_TENSORFLOW
        assert((element*mRows*mCols +row*mCols)*sizeof(T)<mNBytes);
        return mData + element*mRows*mCols+row*mCols; 
#else
        return nullptr;
#endif
    }

    int ndim() const { return 3; }
    size_t batchSize() const { return mBatchSize; }
    
    std::string asString(size_t example) const {
        std::stringstream ss;
        for (size_t r=0;r<rows(); ++r) {
            T* row_ptr = row(example, r);
            if (row_ptr) {
                for (size_t c=0;c<cols(); ++c)
                    ss << row_ptr[c] << " ";
            }
            ss << std::endl;
        }
        return ss.str();
    }


private:
    InputTensorItem::DataType mDataType;
#ifdef USE_TENSORFLOW
    tensorflow::Tensor *mT;
    bool mPrivateTensor;
#endif
    T *mData;
    size_t mBatchSize;
    size_t mRows;
    size_t mCols;
    size_t mNBytes;
};

#endif // TENSORHELPER_H
