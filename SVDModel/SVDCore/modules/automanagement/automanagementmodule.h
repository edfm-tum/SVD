#ifndef AUTOMANAGEMENTMODULE_H
#define AUTOMANAGEMENTMODULE_H

#include "modules/module.h"
#include "grid.h"
#include "transitionmatrix.h"
#include "expression.h"
#include "automanagementout.h"


/// The SimpleManagementModule implements a basic management regime
///
class AutoManagementModule: public Module
{
public:
    AutoManagementModule(std::string module_name, std::string module_type);

    void setup();

    std::vector<std::pair<std::string, std::string> > moduleVariableNames() const;
    virtual double moduleVariable(const Cell *cell, size_t variableIndex) const;

    void run();

    // access
private:
    std::pair<int,int> runArea(const RectF &cell, double p_burnin, double mgmt_cap);
    // logging
    std::shared_ptr<spdlog::logger> lg;

    // store for transition probabilites for affected cells
    TransitionMatrix mMgmtMatrix;

    Expression mBurnInProbability;

    double mMinHeight; ///< stands < min height are never managed

    size_t miIncrementThreshold;
    size_t miManagement;

    std::vector<int> mStateHistogram;

    /// grid with year of last management operation
    Grid<short int> mGrid;
    Grid<double> mManagementCapGrid;
    double mManagementCapModifier {1};

    friend class AutoManagementOut;
};

#endif // AUTOMANAGEMENTMODULE_H
