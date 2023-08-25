#ifndef AUTOMANAGEMENTMODULE_H
#define AUTOMANAGEMENTMODULE_H

#include "modules/module.h"
#include "transitionmatrix.h"
#include "expression.h"


/// The SimpleManagementModule implements a basic management regime
///
class AutoManagementModule: public Module
{
public:
    AutoManagementModule(std::string module_name);

    void setup();

    std::vector<std::pair<std::string, std::string> > moduleVariableNames() const;
    virtual double moduleVariable(const Cell *cell, size_t variableIndex) const;

    void run();

    // access
private:
    // logging
    std::shared_ptr<spdlog::logger> lg;

    // store for transition probabilites for affected cells
    TransitionMatrix mMgmtMatrix;

    Expression mBurnInProbability;

    double mMinHeight; ///< stands < min height are never managed

    size_t miIncrementThreshold;
    size_t miManagement;

};

#endif // AUTOMANAGEMENTMODULE_H
