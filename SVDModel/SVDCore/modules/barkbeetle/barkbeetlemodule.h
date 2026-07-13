#ifndef BARKBEETLEMODULE_H
#define BARKBEETLEMODULE_H

#include "modules/module.h"
#include <stack>

#include "transitionmatrix.h"
#include "grid.h"
#include "expression.h"


/**
 * @brief The SBeetleCell struct is cell level information used by the bark beetle module.
 * The struct uses only 4 bytes per cell.
 */
struct SBeetleCell {
    SBeetleCell() = default;
    uint8_t outbreak_age {0}; ///< outbreak age
    uint8_t n_disturbance {0}; ///< cumulative number of bark beetle disturbance per pixel
    short int last_attack {0}; ///< last simulation year the cell was affected by bark beetles
};


/**
 * @brief The BarkBeetleModule class
 *
 * implements spruce bark beetle (Ips typographus) disturbance in SVD.
 */
class BarkBeetleModule : public Module
{
public:
    BarkBeetleModule(std::string module_name, std::string module_type);
    void setup() override;

    std::vector<std::pair<std::string, std::string> > moduleVariableNames() const override;
    double moduleVariable(const Cell *cell, size_t variableIndex) const override;

    void run() override;
private:
    // logging
    std::shared_ptr<spdlog::logger> lg;

    // functions
    void initialRandomInfestation();
    void windBeetleInteraction();
    double backgroundInfestationProb(const Cell &cell) const;

    void spread();

    void setupKernels(const std::string &file_name, int offspring_factor);
    /// return index of kernel for a given number of bark beetle generations (1, 1.5, 2, 2.5, 3)
    size_t kernelIndex(double gen_count) const;

    // variable indices in climate data
    int miVarBBgen {-1};
    int miVarFrost {-1};

    // variables for states
    state_t miSusceptibility { 0 }; ///< susceptibility of a state based on spruce proportion and height (preprocessed)

    // index of spruce
    int miSpruce {-1};

    // parameters
    double mSuccessOfColonization; ///< probability scaling factor for suseceptibilty (i.e. p(colonization) = susceptibility * SucessOfColonization
    Expression mBackgroundProbFormula; ///< climate sensitive background probability of infestation
    double *mBackgroundProbVar;
    double mWindInteractionFactor; ///< prob. of wind-attaced trees to turn bb infected


    /// storage for the kernels: kernel[ generation_index ] -> vector with rel. distance (Point) and value (probability)
    std::vector< std::vector< std::pair< Point, double >>> mKernels;

    /// grid specific for bark beetles (native 100m resolution)
    Grid<SBeetleCell> mGrid;


    Grid<double> mRegionalBackgroundProb; ///< large scale grid with background infestation probabilities


    std::stack<int> &activeCellsNow()  { return mActiveIsA ? mActiveCellsA : mActiveCellsB; }
    std::stack<int> &activeCellsNextYear()  { return mActiveIsA ? mActiveCellsB : mActiveCellsA; }
    void switchActiveCells() { mActiveIsA = !mActiveIsA; }
    // active cells are stored in two stacks: one is for the current year, the other for the next
    // year; they are switched
    std::stack<int> mActiveCellsA;
    std::stack<int> mActiveCellsB;
    bool mActiveIsA {true};


    /// store for transition probabilites for affected cells
    TransitionMatrix mBBMatrix;

    struct SBeetleStats {
        int n_background {0};
        int n_wind_infestation {0};
        int n_impact {0};
        int n_active_yearend {0};
    } mStats;


    friend class BarkBeetleOut;
};

#endif // BARKBEETLEMODULE_H
