#include "statehistout.h"

#include "model.h"


StateHistOut::StateHistOut()
{
    setName("StateHist");
    setDescription("Outputs a frequency distribution of states over the landscape.\n\n" \
                   "### Parameters\n" \
                   " * `lastFireGrid.filter`: a grid is written only if the expression evaluates to `true` (with `year` as variable). A value of 0 deactivates the grid output.");
    // define the columns
    columns() = {
    {"year", "simulation year", DataType::Int},
    {"stateId", "state Id", DataType::Int},
    {"n", "number of cells that are currently in the state `stateId`", DataType::Int}   };

}

void StateHistOut::setup()
{
    openOutputFile();
}

void StateHistOut::execute()
{

    auto state_count = Model::instance()->states()->stateHistogram();
    int year = Model::instance()->year();
    // write output table
    // Note: it works to use the the index here as state-id: this is exactly the other way round as it is used when saving
    for (size_t i=0;i<state_count.size();++i) {
        if (state_count[i]>0) {
            out() << year << i << state_count[i];
            out().write();
        }
    }
}
