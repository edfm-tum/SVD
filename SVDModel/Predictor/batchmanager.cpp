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
#include "batchmanager.h"
#include "batchdnn.h"
#include "tensorhelper.h"

#include "model.h"
#include "settings.h"
#include "modules/module.h"
#include "filereader.h"
#include "tools.h"

#include <mutex>
#include <algorithm>


#include "strtools.h"

BatchManager *BatchManager::mInstance = nullptr;







BatchManager::BatchManager()
{
    if (mInstance!=nullptr)
        throw std::logic_error("Creation of batch manager: instance ptr is not 0.");
    mInstance = this;
    if (spdlog::get("dnn"))
        spdlog::get("dnn")->debug("Batch manager created: {}", static_cast<void*>(this));

}

BatchManager::~BatchManager()
{
    // delete all batches and free memory
    for (auto b : mBatches)
        delete b;

    if (auto lg = spdlog::get("dnn"))
        lg->debug("Batch manager destroyed: {x}", static_cast<void*>(this));

    mInstance = nullptr;
}

void BatchManager::setup()
{

    lg = spdlog::get("dnn");
    if (!lg)
        throw std::logic_error("BatchManager::setup: logging not available.");
    lg->info("Setup of batch manager.");
    Model::instance()->settings().requiredKeys("dnn", {"batchSize", "maxBatchQueue", "metadata"});
    mBatchSize = Model::instance()->settings().valueUInt("dnn.batchSize");
    mMaxQueueLength = Model::instance()->settings().valueUInt("dnn.maxBatchQueue");


}

void BatchManager::newYear()
{
    mSlotRequested = false;
}

static std::mutex batch_mutex;
std::pair<Batch *, size_t> BatchManager::validSlot(Module *module)
{
    std::pair<Batch *, size_t> result;
    int sleeps = 0;
    do {
        {
            std::lock_guard<std::mutex> guard(batch_mutex);
            mSlotRequested = true;
            result = findValidSlot(module);
        }
        if (!result.first) {
            // wait without holding the batch_mutex lock
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            Model::instance()->processEvents();

            if (++sleeps % 1000 == 0) // 1s
                lg->trace("BatchManager: no batch available (queue full). Sleeping for {} s.", sleeps/1000);

            if (RunState::instance()->cancel() ) {
                lg->info("Canceled.");
                return std::pair<Batch*, int>(nullptr, 0);

            }
            if ( sleeps % (30*60*1000) == 0) { // wait half an hour
                lg->error("time out in batch manager - no empty slots found.");
                return std::pair<Batch*, int>(nullptr, -1);

            }
        }
    } while (!result.first);
    return result;
}

BatchDNN *BatchManager::createDNNBatch()
{
    BatchDNN *b = new BatchDNN(mBatchSize);

    return b;

}

std::pair<Batch *, size_t> BatchManager::findValidSlot(Module *module)
{
    // this function is serialized (access via validSlot() ).

    // look for a batch which is currently not in the DNN processing chain
    Batch *batch = nullptr;
    for (const auto &b : mBatches) {
        if (b->module()==module && b->state()==Batch::Fill && b->freeSlots()>0) {
            batch=b;
            break;
        }
    }
    if (!batch || batch->freeSlots()<=0) {
        Batch::BatchType type = module ? module->batchType() : Batch::DNN;
        if (type == Batch::DNN) {
            size_t dnn_batches = std::count_if(mBatches.begin(), mBatches.end(),
                [](const Batch *b) { return b->type() == Batch::DNN; });
            if (dnn_batches >= mMaxQueueLength) {
                // currently we don't find a proper place for the data in a DNN batch.
                return std::pair<Batch*, size_t>(nullptr, 0);
            }
        }
        // create a new batch; the default (forest) is a batch for DNN
        batch = createBatch(type);
        batch->setModule(module);
        mBatches.push_back( batch );
        lg->trace("created a new batch. Now the list contains {} batch(es).", mBatches.size());
    }


    // get a new slot in the batch
    std::pair<Batch *, size_t> result;
    result.first = batch;
    result.second = batch->acquireSlot();
    if (result.second==0) {
        lg->trace("Started to fill batch [{}] (first slot acquired)", static_cast<void*>(batch));
    }
    return result;


}


Batch *BatchManager::createBatch(Batch::BatchType type)
{
    Batch *b = nullptr;
    switch (type) {
    case Batch::DNN:
        b = createDNNBatch();
        break;
    case Batch::Simple:
        b = new Batch(mBatchSize);
        break;
    default: throw std::logic_error("BatchManager:createBatch: invalid batch type!");
    }

    return b;
}


