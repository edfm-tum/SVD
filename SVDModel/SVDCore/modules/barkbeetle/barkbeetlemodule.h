#ifndef BARKBEETLEMODULE_H
#define BARKBEETLEMODULE_H

#include "modules/module.h"

#include "transitionmatrix.h"
#include "grid.h"
#include "expression.h"



class BarkBeetleModule : public Module
{
public:
    BarkBeetleModule(std::string module_name);
    void setup() override;

    std::vector<std::pair<std::string, std::string> > moduleVariableNames() const override;
    double moduleVariable(const Cell *cell, size_t variableIndex) const override;

    void run() override;
private:
    // logging
    std::shared_ptr<spdlog::logger> lg;

    int miVarBBgen {-1};
    int miVarFrost {-1};

};

#endif // BARKBEETLEMODULE_H
