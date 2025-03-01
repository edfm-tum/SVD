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
#include "states.h"

#include "model.h"
#include "tools.h"
#include "strtools.h"
#include "filereader.h"
#include "randomgen.h"

#include "modules/module.h"

std::vector<std::string> State::mValueNames;

States::States()
{

}

void States::setup()
{
    std::string file_name = Tools::path(Model::instance()->settings().valueString("states.file"));
    FileReader rdr(file_name);
    rdr.requiredColumns({"stateId", "composition", "structure", "fct", "type"});

    size_t i_name = rdr.columnIndex("name");
    size_t i_color = rdr.columnIndex("color");

    state_t max_state=0;
    while (rdr.next()) {
        // read line
        state_t id = state_t( rdr.value("stateId") );
        mStates.push_back( State(id,
                                 rdr.valueString("composition"),
                                 int(rdr.value("structure")),
                                 int(rdr.value("fct")),
                                 (rdr.valueString("type")))
                           );
        mStateSet.insert({id, mStates.size()-1}); // save id and index
        if (i_name != std::numeric_limits<std::size_t>::max())
            mStates.back().setName(rdr.valueString(i_name));
        if (i_color != std::numeric_limits<std::size_t>::max())
            mStates.back().setColorName(rdr.valueString(i_color));

        max_state = std::max(max_state, mStates.back().id());

    }
    // populate lookup table
    mStateIdLookup.resize(max_state+1);
    for (State &s : mStates)
        mStateIdLookup[ s.id() ] = &s; // pointer to state

    spdlog::get("setup")->debug("Loaded {} states from file '{}' (max stateId: {}).", mStates.size(), file_name, max_state);

    // load extra properties
    file_name = Model::instance()->settings().valueString("states.extraFile");
    if (!file_name.empty()) {
        file_name = Tools::path(file_name);
        spdlog::get("setup")->debug("Loading extra state properties from file '{}'...", file_name);
        loadProperties(file_name);
    }


    mStateHistogram.resize(static_cast<size_t>(max_state+1));



}

bool States::loadProperties(const std::string &filename)
{
    FileReader rdr(filename);
    rdr.requiredColumns({"stateId"});
    size_t sidx = rdr.columnIndex("stateId");
    int lineno=0;
    bool has_errors = false;
    while (rdr.next()) {
        ++lineno;
        try {

            if (!Model::instance()->states()->isValid(static_cast<state_t>(rdr.value(sidx))))
                throw std::logic_error("state invalid");

            const State &state = Model::instance()->states()->stateById(static_cast<state_t>(rdr.value(sidx)));
            for (size_t i=0;i<rdr.columnCount();++i)
                if (i != sidx)
                    const_cast<State&>(state).setValue(rdr.columnName(i), rdr.value(i));

        } catch (const std::logic_error &) {
            spdlog::get("setup")->error("loading of properties: state {} is not valid (line {})!", rdr.value(sidx), lineno);
            has_errors = true;
        }
    }
    spdlog::get("setup")->debug("Loaded {} values from file '{}'. States have the following properties: {}", rdr.columnCount()-1, filename, join(State::valueNames()));
    if (has_errors)
        throw logic_error_fmt("Error in loading of extra state properties from '{}'. Check the log.", filename);

    return true;

}

void States::updateStateHistogram()
{

    // reset counter
    std::fill(mStateHistogram.begin(), mStateHistogram.end(), 0);

    // count every state on the landscape
    for (auto &c : Model::instance()->landscape()->cells())
        mStateHistogram[static_cast<size_t>(c.stateId())]++;
}

const State &States::randomState() const
{
    size_t i = static_cast<size_t>(  irandom(1, static_cast<int>(mStates.size())) ); // do not return state 0 = invalid
    if (mStates[i].id() == 0)
        return mStates[1]; // just for debug
    return mStates[i];
}

/*
const State &States::stateById(state_t id)
{

    auto s = mStateSet.find(id);
    if (s!=mStateSet.end()) {
        if (s->second>=mStates.size())
            throw std::logic_error("Invalid state id: " + to_string(id));
        return stateByIndex(s->second);
    }
    throw std::logic_error("Invalid state id: " + to_string(id));

}*/

bool States::registerHandler(Module *module, const std::string &handler)
{
    if (mHandlers.find(handler) != mHandlers.end()) {
        spdlog::get("setup")->error("Cannot register the module '{}' for type '{}': already registered module ('{}')", module->name(), module->stateType(), mHandlers.find(handler)->second->name());
        return false;
    }
    mHandlers[handler] = module;
    spdlog::get("setup")->info("Registered module '{}' for type {}.", module->name(), module->stateType());
    return true;
}

void States::updateStateHandlers()
{
    auto lg = spdlog::get("setup");
    for (auto &s : mStates) {
        // set for each state the stored handler (nullptr can be set too!)
        Module *m = mHandlers[s.moduleString()];
        if (m == nullptr && s.type() != State::Forest && s.type() != State::None ) {
            lg->error("State with Id '{}' requires handling module {}, but this is not available.", s.id(), s.type());
            throw std::logic_error("Expected handling module not found.");
        }
        s.setModule(m);
    }
    if (lg->should_log(spdlog::level::trace)) {
        for (auto &mn : Module::moduleNames()) {
            Module *m = Model::instance()->moduleByName(mn);
            std::vector<std::string> ids;
            if (m) {
                for (auto &s : mStates)
                    if (s.module() == m)
                        ids.push_back(to_string(s.id()));

                lg->trace("States handled by module '{}' (n={}): {}", m->name(), ids.size(), join(ids, ","));
            }
        }
    }
}

State::State(state_t id, std::string composition, int structure, int function, std::string handling_module)
{
    mId = id;
    mComposition = trimmed(composition); // remove trailing spaces if available
    mStructure = structure;
    mFunction = function;
    mHandlingModule = handling_module;
    // this is not ideal:
    // if module is empty, than we assume it is a forest state
    // if not we say it is "None"???? (see setModule())
    if (handling_module.empty() || handling_module=="NA")
        mType = Forest;
    else
        mType = None;

    mModule = nullptr;

    if (mType != State::Forest) {
        // no detailed setup necessary
        return;
    }


    auto species = Model::instance()->species();
    mSpeciesProportion = std::vector<double>(species.size(), 0.);

    // special cases:
    if (mComposition == "mix" || mComposition=="unforested")
        return;

    int dominant_species_index = -1;
    int admixed_species_index[5];
    for (int i=0;i<5;++i) admixed_species_index[i]=-1;

    auto comp_species = split(mComposition, ' ');
    int adm_index = 0;

    for (const auto &s : comp_species) {
       auto it = std::find(species.begin(), species.end(), lowercase(s));
       if (it == species.end())
           throw std::logic_error("Invalid state: species '" + s + "' not found. Id: " + to_string(id));

       if (s == lowercase(s)) {
           // save the index of the admixed species
           admixed_species_index[adm_index++] = static_cast<int>(it-species.begin());
           if (adm_index>5)
               throw std::logic_error("Invalid state: too many admixed species. id: " + to_string(id) + ", " + mComposition);
       } else {
           // the dominant species
           if (dominant_species_index!=-1)
               throw std::logic_error("Invalid state: more than 1 dominant species: id: " + to_string(id) + ", " + mComposition);
           dominant_species_index = static_cast<int>(it-species.begin());
       }
    }

    /* apply some rules:
     * (a) only 1 dominant species: 100%
     * (b) 1 dom. and 1 other: 67/33
     * (c) only 1 other: 50
     * (d) two other: 50/50
     * (e) three other: 33/33/33
     * (f) 4 other: 4x 25
     * none: 0
     * */


    if (dominant_species_index>-1) {
        if (admixed_species_index[0]==-1) {
           mSpeciesProportion[static_cast<size_t>(dominant_species_index)]=1.; // (a)
        } else {
            // max 1 other species: >66% + >20% -> at least 86% -> no other species possible
           mSpeciesProportion[static_cast<size_t>(dominant_species_index)]=0.67; // (b)
           mSpeciesProportion[static_cast<size_t>(admixed_species_index[0])]=0.33;
        }
    } else {
        // no dominant species
        int n_s = 0;
        for (int i=0;i<5;++i)
            if (admixed_species_index[i]>-1) ++n_s;

        double f=0.;
        switch (n_s) {
        case 0: f=0.; break;
        case 1: f=0.5; break;
        case 2: f=0.5; break;
        case 3: f=0.33;  break;
        case 4: f=0.25; break;

        }

        // apply cases
        for (int i=0;i<n_s;++i)
            mSpeciesProportion[static_cast<size_t>(admixed_species_index[i])]=f;

    }


}

std::string State::asString() const
{
    if (!mName.empty())
        return mName;
    std::stringstream s;
    s << compositionString() << " S" << structure() << " F" << function();
    return s.str();
}

void State::setValue(const std::string &name, double value)
{
    int index = valueIndex(name);
    if (index<0) {
        // add property to the list of properties
        mValueNames.push_back(name);
        index = valueIndex(name);
    }
    setValue(index, value);

}

void State::setValue(const int index, double value)
{
    // check if array is large enough
    if (mValues.size() != mValueNames.size()) {
        mValues.resize(mValueNames.size());
    }
    if (index<0 || static_cast<size_t>(index)>=mValueNames.size()) {
        throw std::logic_error("State:setValue: invalid variable index!");
    }
    mValues[static_cast<size_t>(index)] = value;
}
