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
#ifndef CELL_H
#define CELL_H
#include "grid.h"
#include "states.h"

class StateMatrixOut; // forward
class EnvironmentCell; // forward

class Cell
{
public:
    // constructors
    Cell() : mCellIndex(-1), mStateId(-1), mResidenceTime(-1), mNextUpdateTime(-1),
        mNextStateId(-1),  mExternalSeedType(-1), mIsUpdated(false),
        mState(nullptr), mEnvCell(nullptr) {}

    Cell(state_t state, restime_t res_time=0): mCellIndex(-1), mStateId(state), mResidenceTime(res_time), mNextUpdateTime(0),
        mNextStateId(-1), mExternalSeedType(-1), mIsUpdated(false),
        mEnvCell(nullptr) { setState(state); }
    /// establish the link to the environment cell
    void setEnvironmentCell(const EnvironmentCell *ec) { mEnvCell = ec; }
    void setCellIndex(int cell_index) { mCellIndex = cell_index; }
    static void setup(); ///< static setup function, only called once

    // access
    /// isNull() returns true if the cell is not an actively simulated cell
    bool isNull() const { return mStateId==-1; }
    /// the numeric ID of the state the cell is in
    state_t stateId() const { return mStateId; }
    /// get the State object the cell is in;
    /// do not use to check if the cell is part of the simulated landscape! (use isNull() instead)
    const State *state() const { return mState; }
    /// the time (number of years) the cell is already in the current state
    restime_t residenceTime() const { return mResidenceTime; }
    /// get the year for which the next update is scheduled
    int nextUpdate() const {return mNextUpdateTime; }
    /// the index is the position of the cell within the landscape
    int cellIndex() const { return mCellIndex; }
    /// elevation (m) of the cell (from a elevation model)
    float elevation() const;

    /// ptr of the environment cell
    const EnvironmentCell *environment() const { return mEnvCell; }

    /// returns true if the cell should be updated in the current year (i.e. if the DNN should be executed)
    bool needsUpdate() const;

    // actions
    /// check if update is scheduled (i.e. a state change should happen) and in case apply the update;
    /// after update(), the cell is in the final state of the current year ("31st of december")
    void update();
    void setState(state_t new_state);
    void setResidenceTime(restime_t res_time) { mResidenceTime = res_time; }

    /// set a future state update. This is used by both DNN and modules.
    void setNextStateId(state_t new_state) { if(!mIsUpdated) mNextStateId = new_state; }
    /// set a future time. This is used by both DNN and modules.
    void setNextUpdateTime(int next_year) { if(!mIsUpdated) mNextUpdateTime = next_year; }
    /// sets a new state immediately (later updates from DNN are blocked)
    void setNewState(state_t new_state);
    void setInvalid() { mStateId=0; mResidenceTime=0; mState=nullptr; }

    bool hasExternalSeed() const { return mExternalSeedType>0 || (state()!=nullptr && !isNull()); }
    /// set external forest type:
    void setExternalSeedType(int new_type) { mExternalSeedType = new_type; }
    /// get external seed type
    int externalSeedType() const { return mExternalSeedType; }
    void setExternalState(state_t state);

    /// get a vector with species shares (local, mid-range) for the current cell
    std::vector<double> neighborSpecies() const;

    /// get frequency of neighbor cells with a specific state
    double stateFrequencyLocal(state_t stateId) const;
    double stateFrequencyIntermediate(state_t stateId) const;
    double stateFrequencyGlobal(state_t stateId) const;
    double minimumDistanceTo(state_t stateId) const;

    /// retrieve mean height increment over the last years (m)
    /// this facilitates current state change and state history
    double heightIncrement() const;

    /// get history for state/residence time
    const state_t *stateHistory() const { return mHistory.state; }
    const restime_t *resTimeHistory() const { return mHistory.restime; }
    /// the number of elements the state / restime history stores
    static size_t historySize() { return History::NSteps; }

private:
    void dumpDebugData();
    int mCellIndex; ///< index of the grid cell within the landscape grid
    state_t mStateId; ///< the numeric ID of the state the cell is in
    restime_t mResidenceTime;
    int mNextUpdateTime; ///< the year (see Model::year()) when the next update of this cell is scheduled
    state_t mNextStateId; ///< the new state scheduled at mNextUpdateTime
    int mExternalSeedType; ///< if the cell is outside of the project area, this type refers to the forest type
    bool mIsUpdated; ///< flag indicating that the state is already updated (e.g. by management)

    const State *mState; ///< ptr to the State the cell currently is in
    const EnvironmentCell *mEnvCell; ///< ptr to the environment

    // state history

    struct History {
        enum {NSteps=3};
        state_t state[NSteps];
        restime_t restime[NSteps];
        History() {for (int i=0;i<NSteps;++i) { state[i]=0; restime[i]=0; }}
        void saveHistory(state_t newstate, restime_t newtime) {
            for (int i=NSteps-2;i>=0;--i) {
                restime[i+1] = restime[i];
                state[i+1] = state[i];
            }
            restime[0] = newtime;
            state[0] = newstate;
        }

    } mHistory;

    static const std::vector<Point> mLocalNeighbors;
    static const std::vector<Point> mMediumNeighbors;

    /// link to state matrix output
    static StateMatrixOut *mSMOut;


};

#endif // CELL_H
