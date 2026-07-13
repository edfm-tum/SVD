#ifndef STATEMATRIXOUT_H
#define STATEMATRIXOUT_H

#include "output.h"
#include "states.h"

#include <map>

class StateMatrixOut : public Output
{
public:
    StateMatrixOut();
    void setup();
    void execute();

    // add a single state transition to the matrix
    void add(state_t from, state_t to) { mSparseMatrix[std::pair<state_t, state_t>(from, to)]++; }

private:
    std::map< std::pair<state_t, state_t>, int> mSparseMatrix;
    int mInterval {1};
};

#endif // STATEMATRIXOUT_H
