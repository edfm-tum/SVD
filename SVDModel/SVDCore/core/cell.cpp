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
#include "cell.h"
#include "model.h"

// 37 values, roughly a circle with 7px diameter
const std::vector<Point> Cell::mMediumNeighbors = {
              {-1,-3}, {0,-3}, {1,-3},
        {-2,-2}, {-1,-2},{0,-2},{1,-2},{2,-2},
    {-3,-1}, {-2,-1},{-1,-1},{0,-1},{1,-1},{2,-1},{3,-1},
    {-3,0}, {-2,0}, {-1,0},          {1,0}, {2,0}, {3,0},
    {-3,1}, {-2,1} ,{-1,1} ,{0,1} ,{1,1} ,{2,1} ,{3,1},
           {-2,2}, {-1,2},{0,2},{1,2},{2,2},
               {-1,3}, {0,3}, {1,3},

};

// local neighbors: the moore neighborhood (8 values)
const std::vector<Point> Cell::mLocalNeighbors = {
    {-1,-1}, { 0,-1}, { 1,-1},
    {-1, 0},          { 1, 0},
    {-1, 1}, { 0, 1}, { 1, 1}
};




float Cell::elevation() const
{
    return Model::instance()->landscape()->elevationOf(cellIndex());
}

bool Cell::needsUpdate() const
{
    if (Model::instance()->year() >= mNextUpdateTime)
        return true;
    return false;
}

void Cell::update()
{
    // is called at the end of the year: the state changes
    // already now so that we will have the correct state at the
    // start of the next year
    int year = Model::instance()->year();
    if (year+1 >= mNextUpdateTime) {
        // change the state of the current cell
        if (mNextStateId != stateId()) {
            setState( mNextStateId );
            setResidenceTime( 0 );
        } else {
            // the state is not changed;
            // nonetheless, the cell will be re-evaluated in the next year
            mResidenceTime++; // TODO: check if this messes up something with the DNN?
        }
    } else {
        // no update. The residence time changes.
        mResidenceTime++;
    }
    mIsUpdated = false; // reset flag at the end of the year

}

void Cell::setState(state_t new_state)
{
    if (new_state==0) {
        spdlog::get("main")->error("Attempting to set state=0! Details:");
        dumpDebugData();
        return;
    }
    mStateId = new_state;
    if (new_state<0)
        mState=nullptr;
    else {
        mState = &Model::instance()->states()->stateById(new_state);
    }
}

void Cell::setNewState(state_t new_state)
{
    setNextUpdateTime(Model::instance()->year());
    setNextStateId(new_state);
    mIsUpdated = true;
}

void Cell::setExternalState(state_t state)
{
    // external seed cells have a state ptr, but stateId=-1
    mState = &Model::instance()->states()->stateById(state);
    mStateId = -1;
}

std::vector<double> Cell::neighborSpecies() const
{
    auto &grid =  Model::instance()->landscape()->grid();
    size_t n_species = Model::instance()->species().size();
    Point center = grid.indexOf(cellIndex());
    std::vector<double> result(n_species*2, 0.);
    // local neighbors
    double n_local = 0.;
    for (const auto &p : mLocalNeighbors) {
        if (grid.isIndexValid(center + p) && !grid.valueAtIndex(center + p).isNull()) {
            Cell &cell = grid.valueAtIndex(center + p).cell();
            if ((cell.state() && cell.state()->type()==State::Forest) || cell.externalSeedType()>=0) {
                // note for external seeds:
                // if the cell is in 'species-shares' mode, then state() is null
                // if the cell is in 'state' mode, a (constant) state is assigned
                const auto &shares = cell.state()? cell.state()->speciesShares() : Model::instance()->externalSeeds().speciesShares(cell.externalSeedType());
                for (size_t i=0; i<n_species;++i)
                    result[i*2] += shares[i];
                ++n_local;
            }
        }
    }
    if (n_local>0.)
        for (size_t i=0; i<n_species;++i)
            result[i*2] /= n_local;

    // mid-range neighbors
    double n_mid = 0.;
    for (const auto &p : mMediumNeighbors) {
        if (grid.isIndexValid(center + p) && !grid.valueAtIndex(center + p).isNull()) {
            Cell &cell = grid.valueAtIndex(center + p).cell();
            if ((cell.state() && cell.state()->type()==State::Forest) || cell.externalSeedType()>=0) {
                const auto &shares = cell.state()? cell.state()->speciesShares() : Model::instance()->externalSeeds().speciesShares(cell.externalSeedType());
                for (size_t i=0; i<n_species;++i)
                    result[i*2+1] += shares[i];
                ++n_mid;
            }
        }
    }
    if (n_mid>0.)
        for (size_t i=0; i<n_species;++i)
            result[i*2+1] /= n_mid;

    return result;
}

double Cell::stateFrequencyLocal(state_t stateId) const
{
    auto &grid =  Model::instance()->landscape()->grid();
    Point center = grid.indexOf(cellIndex());
    int n_local = 0;
    int n = 0;
    for (const auto &p : mLocalNeighbors) {
        if (grid.isIndexValid(center + p) && !grid.valueAtIndex(center + p).isNull()) {
            Cell &cell = grid.valueAtIndex(center + p).cell();
            if (cell.stateId() == stateId)
                ++n_local;
            }
            ++n;
        }

    return n>0 ? n_local / static_cast<double>(n) : 0.;
}

double Cell::stateFrequencyIntermediate(state_t stateId) const
{
    auto &grid =  Model::instance()->landscape()->grid();
    Point center = grid.indexOf(cellIndex());
    int n_local = 0;
    int n = 0;
    for (const auto &p : mMediumNeighbors) {
        if (grid.isIndexValid(center + p) && !grid.valueAtIndex(center + p).isNull()) {
            Cell &cell = grid.valueAtIndex(center + p).cell();
            if (cell.stateId() == stateId)
                ++n_local;
            }
            ++n;
        }

    return n>0 ? n_local / static_cast<double>(n) : 0.;
}


// distances from center point (X)
// 4 4 3 4 4
// 4 2 1 2 4
// 3 1 X 1 3
// 4 2 1 2 4
// 4 4 3 4 4
// 1..4: distance classes (50m, 100m, 150m, 200m)
static std::vector<std::pair<Point, float> > dist2state = {
    {{1,0},0.5f }, {{0,1},0.5f }, {{-1,0},0.5f }, {{0,-1},0.5f },  // distances 1
    {{-1,-1},1.f }, {{1,-1},1.f }, {{-1,1},1.f }, {{1,1},1.f }, // distances 2
    {{2,0},1.5f }, {{0,2},1.5f }, {{-2,0},1.5f }, {{0, -2},1.50f }, // distances 3
    {{-2,-2},2.f }, {{-1,-2},2.f }, {{1,-2},2.f }, {{2,-2},2.f }, {{-2,-1},2.f }, {{2,-1},2.f }, // distances 4 (upper half)
    {{-2, 2},2.f }, {{-1, 2},2.f }, {{1, 2},2.f }, {{2, 2},2.f }, {{-2, 1},2.f }, {{2, 1},2.f } // distances 4 (lower half)
};

double Cell::minimumDistanceTo(state_t stateId) const
{
    auto &grid =  Model::instance()->landscape()->grid();
    Point center = grid.indexOf(cellIndex());
    // TODO
    for (const auto &p : dist2state) {
        if (grid.isIndexValid(center + p.first) && !grid.valueAtIndex(center + p.first).isNull()) {
            Cell &c = grid.valueAtIndex(center + p.first).cell();
            if (!c.isNull() && c.state()!=nullptr)
                if (c.stateId() == stateId) {
                    // found a distance
                    return p.second;
                }
        }
    }

    // brute force
    const int max_n = 10;
    float min_dist_sq = max_n*max_n;
    for (int y=-max_n; y<=max_n; ++y)
        for (int x=-max_n; x<=max_n; ++x)
            // look in a circle
            if (x*x + y*y <= max_n*max_n) {
                if (grid.isIndexValid(center + Point(x,y)) && !grid.valueAtIndex(center + Point(x,y)).isNull()) {
                    Cell &c = grid.valueAtIndex(center + Point(x,y)).cell();
                    if (!c.isNull() && c.state()!=nullptr)
                        if (c.stateId() == stateId) {
                            // found a  pixel: (x-0.5)*(x-0.5)=x^2-x+0.25
                            min_dist_sq = std::min(min_dist_sq, x*x-x+0.25f + y*y-y+0.25f);
                        }
                }
            }
    float min_dist = sqrt(min_dist_sq); // training data goes to 1250m distance, unit here is 100m steps
    return min_dist; // value in "cells"

}




void Cell::dumpDebugData()
{
    auto lg = spdlog::get("main");
    PointF coord =  Model::instance()->landscape()->grid().cellCenterPoint( Model::instance()->landscape()->grid().indexOf(cellIndex()) );
    lg->info("Cell {} at {}/{}m:", static_cast<void*>(this), coord.x(), coord.y());
    lg->info("Current state ID: {}, {}, residence time: {}", mStateId, mState ? mState->asString() : "Invalid State", mResidenceTime);
    lg->info("external seed type: {}", mExternalSeedType);
    lg->info("Next state-id: {},  update time: {}", mNextStateId, mNextUpdateTime);

}
