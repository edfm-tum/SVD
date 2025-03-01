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
#include "dnnshell.h"

#include <QThread>
#include <QMetaType>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFutureWatcher>

#include "inferencedata.h"
#include "randomgen.h"
#include "model.h"
#include "batch.h"
#include "batchmanager.h"
#include "dnn.h"

#include <tensorflow/core/public/version.h>

DNNShell::DNNShell()
{
    mThreads = new QThreadPool();
}

DNNShell::~DNNShell()
{
    delete_and_clear(mDNNs);
    delete mThreads;

}


void DNNShell::setup(QString fileName)
{
    mBatchesProcessed = 0;
    mCellsProcessed = 0;
    mProcessing = 0;
    mExecutionCount = 0;

    // setup is called *after* the set up of the main model
    lg = spdlog::get("dnn");

    if (lg)
        lg->info("DNN Setup, config file: {}", fileName.toStdString());

    RunState::instance()->dnnState()=ModelRunState::Creating;

    try {
    mBatchManager = std::unique_ptr<BatchManager>(new BatchManager());
    mBatchManager->setup();
    } catch (const std::exception &e) {
        lg->error("An error occurred during setup of the Batch-Manager (DNN): {}", e.what());
        RunState::instance()->dnnState()=ModelRunState::ErrorDuringSetup;
        return;
    }


    try {
        size_t n_models = Model::instance()->settings().valueUInt("dnn.count", 1);
        lg->info("DNN Setup, starting '{}' DNN instances....", n_models);
        for (size_t i=0;i<n_models;++i) {
            DNN *dnn = new DNN();
            mDNNs.push_back(dnn);
            if (!dnn->setupDNN(mDNNs.size())) {
                RunState::instance()->dnnState()=ModelRunState::ErrorDuringSetup;
                return;
            }
        }


        // wait for the model thread to complete model setup before
        // setting up the inputs (which may need data from the model)

        int nwait=0;
        while (RunState::instance()->modelState() == ModelRunState::Creating) {
            if (++nwait < 10 || (nwait < 100 && nwait % 5 == 0) || nwait % 50 == 0)
            lg->trace("waiting for Model thread ... ({}s)", (nwait/5.));
            QThread::msleep(200);
            QCoreApplication::processEvents();
        }

        if (RunState::instance()->modelState() != ModelRunState::ErrorDuringSetup)
            DNN::setupInput();
        else
            lg->debug("Error during model setup - setup of DNN interrupted.");

    } catch (const std::exception &e) {
        RunState::instance()->dnnState()=ModelRunState::ErrorDuringSetup;
        lg->error("An error occurred during DNN setup: {}", e.what());
        return;
    }
    int n_threads = Model::instance()->settings().valueInt("dnn.threads", -1);
    if (n_threads>-1) {
        mThreads->setMaxThreadCount(n_threads);
        lg->debug("setting DNN threads to {}.", n_threads);
    }
    lg->debug("Thread pool for DNN: using {} threads.", mThreads->maxThreadCount());

    RunState::instance()->dnnState()=ModelRunState::ReadyToRun;


}

void DNNShell::doWork(Batch *batch)
{

    RunState::instance()->dnnState() = ModelRunState::Running;

    if (RunState::instance()->cancel()) {
        batch->setError(true);
        //RunState::instance()->dnnState()=ModelRunState::Stopping; // TODO: main
        Q_EMIT workDone(batch);
        return;
    }
    if (mDNNs.size()==0) {
        lg->error("Cannot execute DNN batch because no DNN is available!");
        RunState::instance()->dnnState() = ModelRunState::Error;
        batch->setError(true);
        Q_EMIT workDone(batch);
        return;
    }


    if (batch->state()!=Batch::Fill)
        lg->error("Batch {} [{}] is in the wrong state {}, size: {}", batch->packageId(), static_cast<void*>(batch), batch->state(), batch->usedSlots());

    batch->changeState(Batch::DNNInference);
    mProcessing++;
    lg->debug("DNNShell: received package {}. Starting DNN (batch: {}, state: {}, active threads now: {}, #processing: {}) ", batch->packageId(), static_cast<void*>(batch), batch->state(), mThreads->activeThreadCount(), mProcessing);

    QtConcurrent::run( mThreads,
                       [](DNNShell *shell, Batch *batch, DNN *dnn){
                            dnn->run(batch);

                            if (!QMetaObject::invokeMethod(shell, "dnnFinished", Qt::QueuedConnection,
                                                           Q_ARG(void*, static_cast<void*>(batch))) ) {
                                batch->setError(true);
                            }
                        }, this, batch, mDNNs[mExecutionCount++ % mDNNs.size()]);
}

void DNNShell::dnnFinished(void *vbatch)
{
    Batch * batch = static_cast<Batch*>(vbatch);
    mProcessing--;
    // get the batch from the future watcher
    //Batch *batch = getFinishedBatch();
    //QFutureWatcher<Batch*> *watcher = static_cast< QFutureWatcher<Batch*> * >(sender());
    if (!batch) {
        RunState::instance()->setError("Error in DNN (no watcher)", RunState::instance()->dnnState());
        return;
    }


    if (batch->hasError()) {
        RunState::instance()->setError("Error in DNN", RunState::instance()->dnnState());
        Q_EMIT workDone(batch);
        QCoreApplication::processEvents();
        return;
    }

    lg->debug("DNNShell: dnn finished, package {}, {} cells. Sending to main.", batch->packageId(), batch->usedSlots());

    mBatchesProcessed++;
    mCellsProcessed += batch->usedSlots();

    batch->changeState(Batch::Finished);

    lg->debug("finished data package {} [{}] (size={})", batch->packageId(), static_cast<void*>(batch), batch->usedSlots());

    //dumpObjectInfo();
    Q_EMIT workDone(batch);
    QCoreApplication::processEvents();

    if (!isRunnig())
        RunState::instance()->dnnState() = ModelRunState::ReadyToRun;


}



bool DNNShell::isRunnig()
{
    return mProcessing > 0;
}

const char *DNNShell::tensorFlowVersion()
{
#ifdef USE_TENSORFLOW
return TF_VERSION_STRING;
#else
    return "No Tensorflow";
#endif

}


