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
#include "dnn.h"

#include "settings.h"
#include "model.h"
#include "tools.h"
#include "tensorhelper.h"
#include "batch.h"
#include "batchdnn.h"
#include "batchmanager.h"
#include "randomgen.h"
#include "fetchdata.h"

#include <fstream>
#include <vector>
#include <iomanip>
#include <queue>

// Initialize static members
std::list<InputTensorItem> DNN::mTensorDef;
Ort::Env DNN::mEnv(ORT_LOGGING_LEVEL_WARNING, "SVDModel");

DNN::DNN()
{
    if (spdlog::get("dnn"))
        spdlog::get("dnn")->debug("DNN created: {}", static_cast<void*>(this));
    mTopK_tf = false;
    mTopK_NClasses = 10;
    mNResTimeCls = 0; mNStateCls = 0;
}

DNN::~DNN()
{
}

// Add this helper declaration to dnn.h:
// std::string findMetadataSectionByTensorName(Settings &mg, const std::string &tensor_name, const std::vector<std::string> &sections);

std::string DNN::findMetadataSectionByTensorName(Settings *mg, const std::string &tensor_name, const std::vector<std::string> &sections)
{
    for (const auto &s : sections) {
        std::string key = "input." + s + ".tensorName";
        // If tensorName is defined, check if it matches the ONNX name
        if (mg->hasKey(key) && mg->valueString(key) == tensor_name) {
            return s;
        }
        // Fallback: if tensorName is missing, default to matching the raw section string directly
        if (!mg->hasKey(key) && s == tensor_name) {
            return s;
        }
    }
    return "";
}

bool DNN::setupDNN(size_t aindex)
{
    lg = spdlog::get("setup"); // use "setup" channel for logging during startup phase
    mIndex = aindex;
    auto settings = Model::instance()->settings();
    if (!lg)
        throw std::logic_error("DNN::setup: logging not available.");
    lg->info("Setup of DNN #{}", aindex);
    settings.requiredKeys("dnn", {"file", "maxBatchQueue", "topKNClasses", "state.name", "state.N", "restime.name", "restime.N", "temperatureState", "temperatureRestime"});

    std::string file = Tools::path(settings.valueString("dnn.file"));
    mTopK_tf = false; // Always false for now, as we use CPU top-k
    mTopK_NClasses = settings.valueUInt("dnn.topKNClasses", 10);
    
    // Output tensor names from settings (optional, can also be detected from model)
    mOutputTensorNames = { settings.valueString("dnn.state.name"), settings.valueString("dnn.restime.name")};
    
    mNStateCls = settings.valueUInt("dnn.state.N");
    if (mNStateCls==0)
         mNStateCls = Model::instance()->states()->states().size(); // default: number of states

    mNResTimeCls = settings.valueUInt("dnn.restime.N");

    lg->info("DNN file: '{}'", file);


    std::string file_lower = lowercase(file);
    if (file_lower.size() < 5 || file_lower.compare(file_lower.size() - 5, 5, ".onnx") != 0) {
        lg->error("The DNN file '{}' does not have the required .onnx extension!", file);
        return false;
    }

    try {
        Ort::SessionOptions session_options;
        // Threading Optimization:
        int intra_threads = settings.valueInt("dnn.threads.intra", 0);
        int inter_threads = settings.valueInt("dnn.threads.inter", 0);
        session_options.SetIntraOpNumThreads(intra_threads);
        session_options.SetInterOpNumThreads(inter_threads);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        bool using_cuda = false;
#ifdef USE_CUDA
        try {
            // Diagnostic: List available providers to see if CUDA is built-in/visible
            std::vector<std::string> providers = Ort::GetAvailableProviders();
            std::string prov_list;
            for (const auto& p : providers) prov_list += p + " ";
            lg->debug("Available ONNX Runtime providers: {}", prov_list);

            OrtCUDAProviderOptions cuda_options;
            
            // Multi-GPU Distribution
            // aindex is 1-based (from mDNNs.size() during setup)
            int gpu_count = settings.valueInt("dnn.gpuCount", 1);
            if (gpu_count > 1) {
                cuda_options.device_id = static_cast<int>((aindex - 1) % static_cast<size_t>(gpu_count));
            } else {
                cuda_options.device_id = 0; // Default to first GPU
            }

            session_options.AppendExecutionProvider_CUDA(cuda_options);
            using_cuda = true;
        } catch (const Ort::Exception& e) {
            lg->warn("Failed to enable CUDA: {}. Falling back to CPU.", e.what());
#ifndef _WIN32
            // On Linux, this is often caused by missing CUDA/cuDNN libraries in LD_LIBRARY_PATH
            char* ld_path = std::getenv("LD_LIBRARY_PATH");
            lg->info("Diagnostic - LD_LIBRARY_PATH: {}", ld_path ? ld_path : "not set");
#endif
        }
#endif

        lg->info("--- DNN Setup Summary (Instance #{}) ---", aindex);
        lg->info("Hardware: {}", using_cuda ? "GPU (CUDA)" : "CPU");
        if (using_cuda) {
            int gpu_count = settings.valueInt("dnn.gpuCount", 1);
            lg->info("GPU Device ID: {} (out of {})", (aindex - 1) % gpu_count, gpu_count);
        }
        lg->info("Threading: intra_op={}, inter_op={}, instance_pool={}", 
                 intra_threads == 0 ? "auto" : std::to_string(intra_threads), 
                 inter_threads == 0 ? "auto" : std::to_string(inter_threads),
                 settings.valueInt("dnn.threads", 2));
        lg->info("Batching: size={}, max_queue={}", settings.valueInt("dnn.batchSize", 1024), settings.valueInt("dnn.maxBatchQueue", 100));
        lg->info("---------------------------------------");

        lg->trace(fmt::runtime("Loading ONNX model..."), intra_threads, inter_threads);
#ifdef _WIN32
        std::wstring wfile(file.begin(), file.end());
        mSession = std::make_unique<Ort::Session>(mEnv, wfile.c_str(), session_options);
#else
        mSession = std::make_unique<Ort::Session>(mEnv, file.c_str(), session_options);
#endif
        lg->trace("Successfully loaded ONNX model!");

        // Extract and verify input/output node names
        size_t num_input_nodes = mSession->GetInputCount();
        Ort::AllocatorWithDefaultOptions allocator;

        mInputNames.clear();
        mInputNodeNames.clear();
        for (size_t i = 0; i < num_input_nodes; i++) {
            auto input_name = mSession->GetInputNameAllocated(i, allocator);
            mInputNames.push_back(std::string(input_name.get()));
        }
        // Stabilize pointers by populating mInputNodeNames AFTER all strings are in mInputNames
        for (const auto &name : mInputNames)
            mInputNodeNames.push_back(name.c_str());

        size_t num_output_nodes = mSession->GetOutputCount();
        mOutputNames.clear();
        mOutputNodeNames.clear();
        for (size_t i = 0; i < num_output_nodes; i++) {
            auto output_name = mSession->GetOutputNameAllocated(i, allocator);
            mOutputNames.push_back(std::string(output_name.get()));
        }
        for (const auto &name : mOutputNames)
            mOutputNodeNames.push_back(name.c_str());


    } catch (const Ort::Exception& e) {
        lg->error("ONNX Runtime error: {}", e.what());
        return false;
    }

    mDummyDNN = false;
    
    // setup output: store ptr only when output is enabled
    double sleep_time=0.;
    while (Model::instance()->outputManager()==nullptr || Model::instance()->outputManager()->isSetup()==false) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); sleep_time+=0.05;
        if (sleep_time>60) { // one minute
            lg->error("DNN setup: output manager not ready after 60 sec - time out.");
            return false;
        }
    }

    lg->info("DNN Setup complete.");
    lg = spdlog::get("dnn"); // continue logging on "dnn" channel
    return true;
}


Batch * DNN::run(Batch *abatch)
{
    BatchDNN *batch = dynamic_cast<BatchDNN*>(abatch);
    if (!batch)
        throw std::logic_error("DNN:run: invalid Batch!");

    try {
        STimer timr(lg, "DNN::run:" + to_string(batch->packageId()));
        lg->debug("DNN#{}: started execution for package {}.", mIndex, batch->packageId());

        // if disabled (in debug mode)
        if (mDummyDNN) {
            lg->debug("DNN in dummy mode... no action");
            state_t new_state;
            for (size_t i=0;i<batch->usedSlots();++i) {
                InferenceData &id=batch->inferenceData(i);
                new_state = id.state();
                restime_t rt = static_cast<restime_t>(irandom(1,12));
                id.setResult(new_state, rt);
            }
            batch->changeState(Batch::FinishedDNN);
            return batch;
        }

        timr.print("before main inference");
        
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> input_tensors;
        
        const std::list<InputTensorItem> &tdef = tensorDefinition();
        size_t tindex=0;
        for (const auto &def : tdef) {
            TensorWrapper *tw = batch->tensor(tindex++);
            std::vector<int64_t> shape;
            shape.push_back(static_cast<int64_t>(batch->batchSize()));
            if (def.ndim >= 1) shape.push_back(static_cast<int64_t>(def.sizeX));
            if (def.ndim >= 2) shape.push_back(static_cast<int64_t>(def.sizeY));
            
            // Handle scalar (ndim=0)
            if (def.ndim == 0) {
                shape.clear(); // ORT scalars have empty shape or [1]
                shape.push_back(1);
            }

            ONNXTensorElementDataType ort_type = mapDataType(def.type);
            
            // Map raw pointers to Ort::Value (zero-copy)
            void* raw_data = tw->getRawDataPtr();
            size_t total_elements = batch->batchSize();
            if (def.ndim >= 1) total_elements *= def.sizeX;
            if (def.ndim >= 2) total_elements *= def.sizeY;

            input_tensors.push_back(Ort::Value::CreateTensor(memory_info, raw_data, total_elements * InputTensorItem::sizeOf(def.type), shape.data(), shape.size(), ort_type));
        }

        // Run inference
        auto output_tensors = mSession->Run(Ort::RunOptions{nullptr}, mInputNodeNames.data(), input_tensors.data(), input_tensors.size(), mOutputNodeNames.data(), mOutputNodeNames.size());

        timr.print("main inference");

        // Extract results
        // We expect at least 2 outputs: State and Residence Time
        if (output_tensors.size() < 2) {
            throw std::logic_error("DNN returned less than 2 output tensors.");
        }

        // We need to map the output tensors back to SVD structures.
        // Assuming output_tensors[0] is State and output_tensors[1] is Residence Time based on metadata.
        float* state_output_ptr = output_tensors[mOutIndexState].GetTensorMutableData<float>();
        float* time_output_ptr = output_tensors[mOutIndexRestime].GetTensorMutableData<float>();
        if (state_output_ptr == nullptr || time_output_ptr == nullptr)
            throw std::logic_error("DNN output does not contain data pointers");

        // Wrap results for getTopClasses
        TensorWrap2d<float> outputs_state_wrap(batch->batchSize(), mNStateCls);
        memcpy(outputs_state_wrap.data(), state_output_ptr, batch->batchSize() * mNStateCls * sizeof(float));
        
        TensorWrap2d<float> outputs_time_wrap(batch->batchSize(), mNResTimeCls);
        memcpy(outputs_time_wrap.data(), time_output_ptr, batch->batchSize() * mNResTimeCls * sizeof(float));

        // use CPU to extract top-k results
        TensorWrap2d<float> scores(batch->batchSize(), mTopK_NClasses);
        TensorWrap2d<int32_t> indices(batch->batchSize(), mTopK_NClasses);

        if (lg->should_log(spdlog::level::trace))
            lg->trace("Running Top-K for package {}:", abatch->packageId());
        
        getTopClasses(outputs_state_wrap, batch->batchSize(), mTopK_NClasses, indices, scores);
        timr.print("topk cpu");

        lg->debug("DNN result (#{}): package {}, {} slots.", mIndex, batch->packageId(), batch->usedSlots());

        // Copy the results of the TopK (states, probabilities, residence times) to the batch
        for (size_t i=0; i<batch->usedSlots(); ++i) {
            float *ostate = scores.example(i);
            float *tstate = batch->stateProbResult(i);
            int32_t *oidx = indices.example(i);
            state_t *tidx = batch->stateResult(i);
            for (size_t r=0;r<mTopK_NClasses;++r) {
                *tstate++ = *ostate++;
                *tidx++ = Model::instance()->states()->stateById(static_cast<state_t>(*oidx++)).id();
            }

            float *otime = outputs_time_wrap.example(i);
            float *ttime = batch->timeProbResult(i);
            for (size_t r=0;r<mNResTimeCls;++r) {
                *ttime++ = *otime++;
            }
        }

        lg->debug("DNN::run finished; package {}", batch->packageId());
        batch->changeState(Batch::FinishedDNN);
        return batch;

    } catch(const std::exception &e) {
        lg->error("Error in DNN: {}", e.what());
        batch->setError(true);
        return batch;
    }
}

ONNXTensorElementDataType DNN::mapDataType(InputTensorItem::DataType type)
{
    switch (type) {
        case InputTensorItem::DT_FLOAT: return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
        case InputTensorItem::DT_INT16: return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16;
        case InputTensorItem::DT_INT32: return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
        case InputTensorItem::DT_INT64: return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
        case InputTensorItem::DT_BOOL: return ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL;
        case InputTensorItem::DT_UINT16: return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16;
        default: return ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    }
}


void DNN::setupInput()
{
    mTensorDef.clear();
    size_t n_models = Model::instance()->settings().valueUInt("dnn.count", 1);
    if (n_models == 0) {
        spdlog::get("dnn")->debug("dnn.count is 0, canceling setup of network metadata.");
        return;
    }
    if (n_models > 1) {
        spdlog::get("dnn")->info("dnn.count is >1, DNN metadata is re-used for every DNN");
    }
    std::string metafilename = Tools::path(Model::instance()->settings().valueString("dnn.metadata"));
    if (!Tools::fileExists(metafilename))
        throw std::logic_error("The metadata file for the DNN (" + metafilename + ") does not exist (specified in 'dnn.metadata')!");

    Settings mg;
    mg.loadFromFile(metafilename);

    auto sections = mg.findKeys("input.", true);
    std::shared_ptr<spdlog::logger> lg = spdlog::get("dnn");
    lg->debug("Found sections: {}", join(sections));

    size_t num_input_nodes = mSession->GetInputCount();

    // Core Fix: Populate mTensorDef strictly following the ONNX mInputNames sequence
    for (size_t i = 0; i < num_input_nodes; ++i) {
        const std::string &target_onnx_name = mInputNames[i];

        std::string section_name = findMetadataSectionByTensorName(&mg, target_onnx_name, sections);
        if (section_name.empty()) {
            throw logic_error_fmt("Model verification failed: Input node '{}' required by ONNX model is missing from SVD metadata config.", target_onnx_name);
        }

        std::string s_prefix = "input." + section_name;
        mg.requiredKeys(s_prefix, {"enabled", "dim", "sizeX", "sizeY", "dtype", "type"});

        if (!mg.valueBool(s_prefix + ".enabled")) {
            throw logic_error_fmt("Model verification failed: Input node '{}' is required by ONNX model but flagged as disabled in metadata.", target_onnx_name);
        }

        // Extract metadata mapping
        InputTensorItem item(target_onnx_name,
                             mg.valueString(s_prefix + ".dtype"),
                             mg.valueUInt(s_prefix + ".dim"),
                             mg.valueUInt(s_prefix + ".sizeX"),
                             mg.valueUInt(s_prefix + ".sizeY"),
                             mg.valueString(s_prefix + ".type"));

        // Validate types directly during array construction
        auto type_info = mSession->GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();

        // Type validation check
        if (tensor_info.GetElementType() != mapDataType(item.type)) {
            throw logic_error_fmt("Model verification failed for input '{}': Data type mismatch between ONNX contract and SVD configuration.", target_onnx_name);
        }

        // Shape validation check: ndim+1 (accounting for dynamic batch indices)
        if (shape.size() != static_cast<size_t>(item.ndim + 1)) {
            throw logic_error_fmt("Model verification failed for input '{}': Expected {} dimensions, but model defines {}.", target_onnx_name, item.ndim + 1, shape.size());
        }

        mTensorDef.push_back(item);
        InputTensorItem *ti = &mTensorDef.back();
        ti->mFetch = FetchData::createFetchObject(ti);
        if (!ti->mFetch) {
            throw std::logic_error("Could not create a fetch object for tensor " + item.name);
        }
        ti->mFetch->setup(&mg, s_prefix, item);
    }

    // Verify Outputs
    //size_t num_output_nodes = mSession->GetOutputCount();
    int i_state = index_of(mOutputNames, Model::instance()->settings().valueString("dnn.state.name"));
    int i_restime = index_of(mOutputNames, Model::instance()->settings().valueString("dnn.restime.name"));
    if (i_state < 0)
        throw logic_error_fmt("Model check failed: Required output node ('dnn.state.name') '{}' not found in model", Model::instance()->settings().valueString("dnn.state.name"));
    if (i_restime < 0)
        throw logic_error_fmt("Model check failed: Required output node ('dnn.restime.name') '{}' not found in model", Model::instance()->settings().valueString("dnn.restime.name"));

    mOutIndexState = i_state;
    mOutIndexRestime = i_restime;

    lg->info("DNN model verification successful: All {} inputs synchronized and matched.", num_input_nodes);
}

TensorWrapper *DNN::buildTensor(size_t batch_size, InputTensorItem &item)
{
    TensorWrapper *tw = nullptr;

    if (item.ndim == 0) {
        switch (item.type) {
        case InputTensorItem::DT_BOOL: {
            tw = new TensorWrap1d<bool>();
            static_cast< TensorWrap1d<bool>* >(tw)->setValue(false);
            break;
        }
        default: break;
        }
    }

    if (item.ndim == 1) {
        switch (item.type) {
        case InputTensorItem::DT_FLOAT:
            tw = new TensorWrap2d<float>(batch_size, item.sizeX); break;
        case InputTensorItem::DT_INT16:
            tw = new TensorWrap2d<int16_t>(batch_size, item.sizeX); break;
        case InputTensorItem::DT_UINT16:
            tw = new TensorWrap2d<uint16_t>(batch_size, item.sizeX); break;
        case InputTensorItem::DT_INT64:
            tw = new TensorWrap2d<int64_t>(batch_size, item.sizeX); break;
        case InputTensorItem::DT_INT32:
            tw = new TensorWrap2d<int32_t>(batch_size, item.sizeX); break;
        default:
            throw std::logic_error("Unhandled data type in tensorwrapper");
        }
    }

    if (item.ndim==2) {
        switch (item.type) {
        case InputTensorItem::DT_FLOAT:
            tw = new TensorWrap3d<float>(batch_size, item.sizeX, item.sizeY); break;
        default: throw std::logic_error("datatype not handled in tensorwrapper");
        }
    }

    if (tw)
        return tw;
    throw std::logic_error("Could not create a tensor.");
}

void DNN::setupBatch(Batch *abatch, std::vector<TensorWrapper *> &tensors)
{
    BatchDNN *batch = dynamic_cast<BatchDNN*>(abatch);
    if (!batch)
        throw std::logic_error("DNN:run: invalid Batch!");

    size_t index=0;
    for (auto &td : mTensorDef) {
        TensorWrapper *tw = buildTensor(batch->batchSize(), td);
        td.index = index++;
        tensors.push_back(tw);
    }
}

class ComparisonClassTopK {
public:
    bool operator() (const std::pair<float, size_t> &p1, const std::pair<float, size_t> &p2) {
        return p1.first>p2.first;
    }
};

void DNN::getTopClasses(TensorWrapper &classes, const size_t batch_size, const size_t n_top, TensorWrapper &indices, TensorWrapper &scores)
{
    std::priority_queue< std::pair<float, size_t>, std::vector<std::pair<float, size_t> >, ComparisonClassTopK > queue;

    size_t n_cls = static_cast<size_t>(mNStateCls);
    TensorWrap2d<float> &cls_dat = static_cast<TensorWrap2d<float>&>(classes);
    TensorWrap2d<int32_t> &res_ind = static_cast<TensorWrap2d<int32_t>&>(indices);
    TensorWrap2d<float> &res_scores = static_cast<TensorWrap2d<float>&>(scores);

    for (size_t i=0; i<batch_size; i++) {
        float *p = cls_dat.example(i);
        for (size_t j=0; j<n_cls; ++j, ++p) {
            if (queue.size()<n_top || *p > queue.top().first) {
                if (queue.size() == n_top)
                    queue.pop();
                queue.push( std::pair<float, size_t>(*p,j));
            }
        }
        // Fill results in descending order (highest score first at index 0)
        // Since we have a min-heap, we pop the smallest of the top-K first.
        // We fill from index queue.size()-1 down to 0.
        int j = static_cast<int>(queue.size()) - 1;
        while( !queue.empty() ) {
            res_ind.example(i)[j] = static_cast<int32_t>(queue.top().second);
            res_scores.example(i)[j] = queue.top().first;
            queue.pop();
            --j;
        }
    }
}

int DNN::chooseProbabilisticIndex(float *values, int n, int skip_index)
{
    double p_sum = 0.;
    for (int i=0;i<n;++i)
        p_sum+= (i!=skip_index ? static_cast<double>(values[static_cast<size_t>(i)]) : 0. );

    double p = nrandom(0., p_sum);

    p_sum = 0.;
    for (int i=0;i<n;++i, ++values) {
        p_sum += (i!=skip_index ? static_cast<double>(*values) : 0. );
        if (p < p_sum)
            return i;
    }
    return n-1;
}
