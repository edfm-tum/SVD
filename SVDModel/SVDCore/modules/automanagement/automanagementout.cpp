#include "automanagementout.h"
#include "automanagementmodule.h"

#include "model.h"

AutoManagementOut::AutoManagementOut()
{
    setName("AutoManagement");
    setDescription("Output of the AutoManagement module. The output contains a histogram of how many cells were managed per state. \n\n" \
                   "Each line is holds a single state; note: only states with at least one affected cell are saved." \
                   );
    // define the columns
    columns() = {
                 {"year", "simulation year", DataType::Int},
                 {"stateId", "unique identifier of a", DataType::Int},
                 {"n", "number of cells affected by management of this state in this year", DataType::Int}   };


}

void AutoManagementOut::setup()
{
    //mLastFire.setExpression(Model::instance()->settings().valueString(key("lastFireGrid.filter")));
    //mLastFirePath = Tools::path(Model::instance()->settings().valueString(key("lastFireGrid.path")));
    openOutputFile();

}

void AutoManagementOut::execute()
{
    AutoManagementModule *am_module = dynamic_cast<AutoManagementModule *>( Model::instance()->module("AutoManagement") );
    if (!am_module)
        throw std::logic_error("Error: AutoManagement-Module not available, but AutoManagement output active!");

    int year = Model::instance()->year();
    auto &state_count = am_module->mStateHistogram;

    // write output table
    // Note: it works to use the the index i here as state-id: this is exactly the other way round as it is used when saving
    for (size_t i=0;i<state_count.size();++i) {
        if (state_count[i]>0) {
            out() << year << i << state_count[i];
            out().write();
        }
    }
}
