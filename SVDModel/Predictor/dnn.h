/********************************************************************************************
**    SVD - the scalable vegetation dynamics model
**    https://github.com/SVDmodel/SVD
**    Copyright (C) 2018-  Werner Rammer, Rupert Seidl
**
**    This program is free software: you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation, either version 3 of the License, or
**    (at your option) any later version.
**
**    This program is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
**    You should have received a copy of the GNU General Public License
**    along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************************************/
#ifndef DNN_H
#define DNN_H

#undef SWIG

#include "spdlog/spdlog.h"
#include <onnxruntime_cxx_api.h>

class Batch; // forward
class Settings; // forward

#include "inputtensoritem.h"
#include "tensorhelper.h"
#include <list>
#include <memory>


class DNN
{
public:
    DNN();
    ~DNN();
    size_t index() const {return mIndex; }

    /// set up the actual DNN
    bool setupDNN(size_t aindex);

    /// set up the links to the main model
    void setupInput();

    static void setupBatch(Batch *abatch, std::vector<TensorWrapper*> &tensors);

    /// DNN main function: execute the DNN inference for the
    /// examples provided in 'batch'.
    Batch *run(Batch *abatch);

    // getters
    /// the definition of the tensors to fill
    static const std::list<InputTensorItem> &tensorDefinition() {return mTensorDef; }

private:

    static TensorWrapper *buildTensor(size_t batch_size, InputTensorItem &item);
    std::string findMetadataSectionByTensorName(Settings *mg, const std::string &tensor_name, const std::vector<std::string> &sections);

    // logging
    std::shared_ptr<spdlog::logger> lg;

    // DNN specifics
    size_t mIndex; ///< internal number of the DNN
    bool mDummyDNN; ///< if true, then the DNN components are not really used (for debug builds)

    bool mTopK_tf; ///< use framework for the state top k calculation (not supported by ORT currently)
    size_t mTopK_NClasses; ///< number of classes used for the top k algorithm
    std::vector<std::string> mOutputTensorNames; ///< names of the output tensors (e.g. output/Softmax)
    size_t mNStateCls; ///< number of output classes for state
    size_t mNResTimeCls; ///< number of classes for residence time
    size_t mOutIndexState; ///< index of state prediction result in output tensors
    size_t mOutIndexRestime; ///< index oof residence time prediction result in output tensors

    /// retrieve the top n classes in "classes" and store results in 'indices' and 'scores'.
    /// this function uses CPU
    void getTopClasses(TensorWrapper &classes, const size_t batch_size, const size_t n_top, TensorWrapper &indices, TensorWrapper &scores);

    /// select randomly an index 0..n-1, with values the weights.
    int chooseProbabilisticIndex(float *values, int n, int skip_index=-1);

    /// definition of input tensors
    static std::list<InputTensorItem> mTensorDef;

    // ONNX Runtime
    static Ort::Env mEnv;
    std::unique_ptr<Ort::Session> mSession;

    std::vector<std::string> mInputNames;
    std::vector<std::string> mOutputNames;
    std::vector<const char*> mInputNodeNames;
    std::vector<const char*> mOutputNodeNames;

    ONNXTensorElementDataType mapDataType(InputTensorItem::DataType type);

};

#endif // DNN_H
