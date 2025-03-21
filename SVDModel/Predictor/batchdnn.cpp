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
#include "batchdnn.h"

#include "tensorhelper.h"

#include "model.h"

#include "randomgen.h"

#include "dnn.h"
#include "fetchdata.h"
#include "statechangeout.h"

// static decl
StateChangeOut *BatchDNN::mSCOut = nullptr;

BatchDNN::BatchDNN(size_t batch_size) : Batch(batch_size)
{
    mType = DNN;

    mInferenceData.resize(mBatchSize);
    // reserve memory for the topK classes for target states and residence time
    const auto &settings = Model::instance()->settings();
    settings.requiredKeys("dnn", {"topKNClasses", "restime.N", "allowStateChangeAtMaxTime", "temperatureState", "temperatureRestime"});
    mNTopK = settings.valueUInt("dnn.topKNClasses", 10);
    mNTimeClasses = settings.valueUInt("dnn.restime.N", 10);
    mAllowStateChangeAtMaxTime = settings.valueBool("dnn.allowStateChangeAtMaxTime", "false");
    mStateTemperature = settings.valueDouble("dnn.temperatureState");
    mRestimeTemperature = settings.valueDouble("dnn.temperatureRestime");


    mStates.resize(mBatchSize * mNTopK);
    mStateProb.resize(mBatchSize * mNTopK);
    mTimeProb.resize(mBatchSize * mNTimeClasses);

    setupTensors();

    mSCOut = dynamic_cast<StateChangeOut*>( Model::instance()->outputManager()->find("StateChange") );
    if (mSCOut && !mSCOut->enabled())
        mSCOut = nullptr;

}

BatchDNN::~BatchDNN()
{
    // free the memory of the tensors...
//    if (spdlog::get("dnn"))
//        spdlog::get("dnn")->trace("Destructor of batch, free tensors");
    for (auto p : mTensors) {
        delete p;
    }

}

void BatchDNN::processResults()
{

    auto lg = spdlog::get("main");

    lg->debug("Model: received package {} [{}](from DNN). Processing data.", packageId(), static_cast<void*>(this));

    // choose from the topK classes
    selectClasses();

    for (size_t i=0;i<usedSlots();++i) {
        inferenceData(i).writeResult();
    }

    // write detailed output for every example in the batch
    if (mSCOut) {
        for (size_t i=0;i<usedSlots(); ++i) {
            if (mSCOut->shouldWriteOutput(inferenceData(i)))
                mSCOut->writeLine(stateChangeOutput(i));
        }
    }


}

bool BatchDNN::fetchPredictors(Cell *cell, size_t slot)
{
    inferenceData(slot).fetchData(cell, this, slot); // the old way
    for (auto &t : DNN::tensorDefinition()) {
        try {
        t.mFetch->fetch(cell, this, slot);
        } catch (const std::logic_error &e) {
            throw std::logic_error("Error fetching data for tensor: " + t.name + ": " + e.what());
        }
    }

    return true;
}

void BatchDNN::setupTensors()
{
    DNN::setupBatch(this, mTensors);
}

// Example code Francois Chollet, Deep Learning Book:
//def sample_next(predictions, temperature=1.0):
//    predictions = np.asarray(predictions).astype("float64")
//    predictions = np.log(predictions) / temperature
//    exp_preds = np.exp(predictions)
//    predictions = exp_preds / np.sum(exp_preds)
//    probas = np.random.multinomial(1, predictions, 1)
//    return np.argmax(probas)

// choose randomly a value in *values (length=n), return the index.
size_t BatchDNN::chooseProbabilisticIndex(float *values, size_t n, double temperature)
{
    if (temperature != 1.) {
        // include temperature parameter
        for (size_t i=0;i<n;++i)
            values[i] = exp( log(values[i]) / temperature );
    }

    // calculate the sum of probs
    double p_sum = 0.;
    for (size_t i=0;i<n;++i)
        p_sum+= static_cast<double>(values[i]);

    double p = nrandom(0., p_sum);

    p_sum = 0.;
    for (size_t i=0;i<n;++i) {
        p_sum += static_cast<double>(values[i]);
        if (p < p_sum)
            return i;
    }
    return n-1;
}


void BatchDNN::selectClasses()
{
    // Now select for each example the result of the prediction
    // choose randomly from the result
    for (size_t i=0; i<usedSlots(); ++i) {
        InferenceData &id = inferenceData(i);

        if (id.nextState() > 0)
            continue; // the state has already been set, e.g. by random states if DNN is not enabled in debug mode.

        // residence time: at least one year, i.e. for 10 classes it will have values between [1,10]
        restime_t rt = static_cast<restime_t>( chooseProbabilisticIndex(timeProbResult(i), mNTimeClasses, mRestimeTemperature )) + 1;

        if (!mAllowStateChangeAtMaxTime) {
            // allowing state change at max time: default = false
            // if false: if #years = maximum -> state stays the same, else: state *has* to change
            // if true: state and time are chosen independently
            if (rt == static_cast<restime_t>(mNTimeClasses)) {
                // the state will be the same for the next period (no change)
                id.setResult(id.state(), rt);
                continue;
            } else {
                // select the next state probalistically
                // the next state is not allowed to stay the same -> set probability to 0
                for (size_t j=0;j<mNTopK;++j) {
                    if (stateResult(i)[j] == id.state()) {
                        stateProbResult(i)[j] = 0.f;
                        break;
                    }
                }
            }
        }

        // select the next state
        size_t index = chooseProbabilisticIndex(stateProbResult(i), mNTopK, mStateTemperature);
        state_t stateId = stateResult(i)[index];

//        if (stateId == 0 || rt == 0) {
//            spdlog::get("main")->error("bad data in batch {} with {} used slots. item {}: update-time: {}, update-state: {} (set state to 1)", packageId(), usedSlots(), i, inferenceData(i).nextTime(), inferenceData(i).nextState());
//            if (stateId==0)
//                stateId = 1;
//            if (rt == 0)
//                rt = 1;
//        }
        id.setResult(stateId, rt);

    }

    auto lg = spdlog::get("dnn");
    if (lg->should_log(spdlog::level::trace)) {
        lg->trace("Dump for package '{}'", mPackageId);
        lg->trace("{}", inferenceData(0).dumpTensorData());
        InferenceData &id = inferenceData(0);

        std::stringstream s;
        s << "Residence time\n";
        float *t = timeProbResult(0);
        for (size_t i=0; i<mNTimeClasses;++i)
            s << i+1 << " yrs: " << (*t++)*100 << "%" << (static_cast<size_t>(id.nextTime()) - static_cast<size_t>(Model::instance()->year())==i+1 ? " ***": "")<< "\n";

        s << "Next update in year: " << id.nextTime();
        s << "\nClassification Results: current State " << id.state() << ": " << Model::instance()->states()->stateById(id.state()).asString() << "\n";
        state_t *st = stateResult(0);
        float *stp = stateProbResult(0);
        float stateprobsum = 0.f;
        for (size_t i=0;i <mNTopK; ++i) {
            if (Model::instance()->states()->isValid(*st)) {
                const State &tstate = Model::instance()->states()->stateById(*st);
                stateprobsum+=*stp;
                s << *st << ": " <<  (*stp)*100.f
                  << "% "<< tstate.asString() << (id.nextState() == *st ? " ***": "") << "\n";
            } else {
                s << "INVALID STATE: " << *st << "p=" << (*stp)*100.f << "\n";
            }
            ++st;
            ++stp;
        }
        s << "Selected State: " << id.nextState()   << " : "
          << (Model::instance()->states()->isValid(id.nextState()) ? Model::instance()->states()->stateById(id.nextState()).asString() : "(invalid)")
          << " - prob. sum: " << stateprobsum * 100.f;
        lg->trace("Results for example 0 in the batch (package {})", mPackageId);
        lg->trace("{}", s.str());


    }


}

/// create an output line for the item 'n'
std::string BatchDNN::stateChangeOutput(size_t index)
{
    std::stringstream s;
    char sep=',';
    // selected state/residence time
    const auto &id = inferenceData(index);

    s << Model::instance()->year() << sep << id.cellIndex() << sep << id.state() << sep << id.cell().residenceTime() << sep << id.nextState() << sep << id.nextTime();
    // states / probs
    float *tprob = timeProbResult(index);
    float *stp = stateProbResult(index);
    state_t *st = stateResult(index);

    for (size_t i=0;i<mNTopK;++i)
        s <<  sep << st[i]
           << sep << stp[i];
   for (size_t i=0;i<mNTimeClasses;++i)
       s << sep << tprob[i];

   return s.str();

}
