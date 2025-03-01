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
#ifndef MODULE_H
#define MODULE_H

#include <memory>

#include "states.h"
#include "../Predictor/batch.h"

class Cell; // forward
class Batch; // forward

/**
 * @brief The Module class
 *
 * This is the base class for modules in SVD. Modules are derived from `Module` and add specific logic.
 * Life-cylce functions: `setup()` is used initalization, `run()` for (annual) execution.
 * You can specfiy module variables (see moduleVariableNames()).
 * If the module handles states exclusively (e.g., MatrixModule), then you `registerModules()` and
 * overload `processBatch()`.
 */
class Module
{

public:
    Module(std::string module_name, std::string module_type, State::StateType type) : mName(module_name), mTypeString(module_type), mStateType(type) {}
    virtual ~Module();
    /// list of available module types
    static std::vector<std::string> &allModuleTypes() {return mModuleTypes; }

    /// names of all created and active modules
    static std::vector<std::string> &moduleNames() {return mModuleNames; }
    static void clearModuleNames() { mModuleNames.clear(); }

    /// create a module as derived class
    /// @param module_name name of the module, also used in configuration
    /// @param module_type type as string, see also allModuleTypes()
    static std::shared_ptr<Module> moduleFactory(std::string module_name, std::string module_type);

    // properties
    const std::string &name() const { return mName; }
    const std::string &typeString() const { return mTypeString; }
    State::StateType stateType() const { return mStateType; }
    Batch::BatchType batchType() const { return mBatchType; }

    /// register the module using the ID and name
    /// this is necessary when the module is handling exclusively one or multiple states
    bool registerModule();

    // actions to overload
    virtual void prepareCell(Cell *) {}
    virtual void processBatch(Batch *) {}

    // startup
    /// function called to set up the module
    virtual void setup() {}
    /// main function that executes the module
    /// modules are called by the model (see runModules())
    virtual void run() {}

    // helpers
    /// returns the full name for a setting within a module.
    /// for example, calling (within module "wind") with subkey="speed" returns "modules.wind.speed"
    std::string modkey(const std::string &subkey) { return "modules." + name() + "." + subkey; }

    // variables
    virtual std::vector<std::pair<std::string, std::string> > moduleVariableNames() const;
    virtual double moduleVariable(const Cell *cell, size_t variableIndex) const;
protected:
    std::string mName;
    std::string mTypeString; ///< the module type as string (e.g. wind, fire, matrix)
    State::StateType mStateType; ///< states of this type are automatically handled by the module
    Batch::BatchType mBatchType; ///< type of the batch used by the module (e.g. DNN or Simple)
    static std::vector<std::string> mModuleNames; ///< names of all created and active modules
    static std::vector<std::string> mModuleTypes; ///< available module types

};

#endif // MODULE_H
