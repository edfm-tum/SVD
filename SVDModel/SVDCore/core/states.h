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
#ifndef STATES_H
#define STATES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "strtools.h"

/// state_t:
typedef short int state_t; // 16bit
typedef short int restime_t; // 16bit

class Module; // forward

class State {
public:
    enum StateType { Forest=0, Matrix=1, None=99 };
    State(state_t id, std::string composition, int structure, int function, std::string handling_module);
    void setName(const std::string &name) { mName = name; }
    void setColorName(const std::string &color) {mColorName = color; }

    /// the handling module for a state
    StateType type() const { return mType; }
    /// unique state id
    state_t id() const { return mId; }
    /// composition of the state; this is a string consisting of one or more species codes
    const std::string &compositionString() const { return mComposition; }
    /// value of the functioning dimension, typcally value for LAI
    int function() const { return mFunction; }
    /// structure dimension of the state. Typically forest top-height in m.
    int structure() const {return mStructure; }

    /// the top height in meters.
    /// it is assumed here that the numeric value given by the structure component of the state
    /// is *already* the top height.
    double topHeight() const { return static_cast<double>(structure()); }
    const std::string &name() const  { return mName; }
    const std::string &colorName() const { return mColorName; }
    std::string asString() const;

    const std::string &moduleString() const { return mHandlingModule; }
    /// set the module handling this state
    void setModule(Module *module) { mModule = module; }
    /// get the handling module for the state
    Module *module() const { return mModule; }

    /// vector of species proportions of the state. The index is based on Model::instance()->species().
    /// e.g., if Model::instance()->species()[7] == "piab" -> speciesShares()[7] -> prop of "piab".
    const std::vector<double> &speciesProportion() const { return mSpeciesProportion; }

    // properties
    static const std::vector<std::string> &valueNames() { return mValueNames; }
    static int valueIndex(const std::string &name) { return indexOf(mValueNames, name); }
    bool hasValue(const std::string &name) const { return indexOf(mValueNames, name)>=0; }
    double value(const std::string &name) const { return value(static_cast<size_t>(valueIndex(name))); }
    double value(const size_t index) const { size_t i=static_cast<size_t>(index); return i<mValues.size() ? mValues[i] : 0.;}
    void setValue(const std::string &name, double value);
    void setValue(const int index, double value);
private:
    state_t mId;
    std::string mComposition;
    int mStructure;
    int mFunction;
    StateType mType;
    std::string mName;
    std::string mHandlingModule; ///< string as provided in the input; mapping to actual modules via setModule()
    std::string mColorName; ///< color for visualization
    std::vector<double> mSpeciesProportion;

    Module *mModule;

    /// store for extra state specific values used by modules
    std::vector<double> mValues;
    static std::vector<std::string> mValueNames;
};


/// Manages and stores all individual states.
class States
{
public:
    States();
    void setup();

    /// load properties from a text file (stateId is the key)
    bool loadProperties(const std::string &filename);
    /// frequency table for every state
    void updateStateHistogram();
    /// frequencies of states in the landscape. use stateId as index for vector.
    const std::vector<int> &stateHistogram() const { return mStateHistogram; }

    // members
    bool isValid(state_t state) const { return mStateSet.find(state) != mStateSet.end(); }
    const State &randomState() const;
    const std::vector<State> &states() { return mStates; }
    const State &stateByIndex(size_t index) const { return mStates[index]; }
    const State &stateById(state_t id) const {
        assert(static_cast<size_t>(id) < mStateIdLookup.size());
        State *s = mStateIdLookup[id];
        if (!s)
            throw std::logic_error("Invalid state id: " + to_string(id));
        return *s; }
    /* const State &stateById(state_t id); */
    /// get the size required for a continuous vector from 0..max-state-id
    state_t stateIdLookupLength() const { return mStateIdLookup.size(); }

    // handlers
    bool registerHandler(Module *module, const std::string &handler);
    /// update the handlers of all states
    void updateStateHandlers();

private:
    std::vector<State> mStates;
    std::unordered_map<state_t, size_t> mStateSet;
    std::map<std::string,  Module*> mHandlers;

    /// for counting states on the landscape
    std::vector<int> mStateHistogram;
    /// quick lookup table for stateIds
    /// stores on positions 0..max_state_id-1 pointers to mStates vector
    std::vector<State*> mStateIdLookup;


};

#endif // STATES_H
