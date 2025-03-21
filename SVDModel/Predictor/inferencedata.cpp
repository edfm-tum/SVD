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
#include "inferencedata.h"
#include "model.h"
#include "batchdnn.h"
#include "tensorhelper.h"
#include "dnn.h"


void InferenceData::fetchData(Cell *cell, BatchDNN *batch, size_t slot)
{
    mOldState = cell->stateId();
    mResidenceTime = cell->residenceTime();
    mNextState = 0;
    mNextTime = 0;
    mIndex = cell->cellIndex();
    //static_cast<int>( cell - Model::instance()->landscape()->grid().begin() );

    mBatch = batch;
    mSlot = slot;

    if (!cell->state() || !cell->environment() || cell->isNull()) {
        throw logic_error_fmt("InferenceData: invalid cell: index: {}, current state: {}, ptr-state: {}, ptr-env: {}",
                              mIndex, mOldState, (void*)cell->state(), (void*)cell->environment());
    }

    // now pull all the data
    //internalFetchData();
}

void InferenceData::setResult(state_t state, restime_t time)
{
    if (state==0) //TODO: Enable again (nur wegen warning!!)
        spdlog::get("main")->error("InferenceData::setResult, state=0!");
    mNextState=state;
    // time is the number of years the next update should happen
    // we change to the absolute year:
    mNextTime=static_cast<restime_t>(Model::instance()->year() + time);
}

void InferenceData::writeResult()
{
    // write back:
    Cell &cell = Model::instance()->landscape()->cell(mIndex);
    cell.setNextStateId(mNextState);
    cell.setNextUpdateTime(mNextTime);


}

const EnvironmentCell &InferenceData::environmentCell() const
{
    return Model::instance()->landscape()->environmentCell(mIndex);
}

const Cell &InferenceData::cell() const
{
    return Model::instance()->landscape()->cell(mIndex);
}

std::string InferenceData::dumpTensorData()
{
    std::stringstream ss;
    const std::list<InputTensorItem> &tdef = DNN::tensorDefinition();
    // todo: cell->enviid, climid....

    ss << " **** Dump for example " << mSlot << " **** " << std::endl;
    ss << " Context: cell-index: " << cell().cellIndex() << ", climateId:" << cell().environment()->climateId() << ", env.id:" << cell().environment()->id() << ", elevation: " << cell().elevation() << std::endl;
    for (const auto &def : tdef) {
        ss << "*****************************************" << std::endl;
        ss << "Tensor: '" << def.name << "', ";
        ss << "dtype: " << InputTensorItem::datatypeString(def.type) << ", Dimensions: " << def.ndim;
        if (def.ndim == 1)
            ss << ", Size: " << def.sizeX << std::endl;
        else
            ss << ", Size: "<< def.sizeX << " x " << def.sizeY << std::endl;
        ss << "*****************************************" << std::endl;
        ss <<  mBatch->tensor(def.index)->asString(mSlot) << std::endl;

    }
    return ss.str();
}

