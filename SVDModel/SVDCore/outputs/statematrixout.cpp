#include "statematrixout.h"

#include "model.h"

StateMatrixOut::StateMatrixOut()
{
    setName("StateMatrix");
    setDescription("Outputs the number of cumulative state transition between any two states.\n\n" \
                   "### Parameters\n" \
                   " * `interval`: output is only generated every `interval` years.");
    // define the columns
    columns() = {
                 {"year", "simulation year", DataType::Int},
                 {"stateId", "state Id of the originating state", DataType::Int},
                 {"stateToId", "state Id of the target state", DataType::Int},
                 {"n", "cumulated number of cells that transitioned from `stateId` to `stateToId`", DataType::Int}   };

}

void StateMatrixOut::setup()
{
    auto lg = spdlog::get("setup");
    mInterval = Model::instance()->settings().valueInt(key("interval"));

    openOutputFile();
}

void StateMatrixOut::execute()
{
    int year = Model::instance()->year();
    if (mInterval>0)
        if (year % mInterval != 1)
            return;

    for (auto const &e : mSparseMatrix) {
        out() << year
              << e.first.first  // state from
              << e.first.second // state to
              << e.second; // n (content)
        out().write();
    }

}
