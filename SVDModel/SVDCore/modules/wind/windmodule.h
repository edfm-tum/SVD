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
#ifndef WINDMODULE_H
#define WINDMODULE_H

#include <map>

#include "modules/module.h"

#include "transitionmatrix.h"
#include "grid.h"
#include "expression.h"

class WindOut; // forward

/**
 * @brief The SWindCell struct is cell level information used by the wind module.
 */
struct SWindCell {
    SWindCell() = default;
    short int last_storm {0}; ///< year when the cell was affected the last time
    short int n_storm {0}; ///< cumulative number of storms per pixel
};

/**
 * @brief The SWindStat struct collects per-event-statistics of wind events.
 */
struct SWindStat {
    int year {-1}; ///< year of the fire
    int Id {-1}; ///< unique identifier
    double x {0}, y {0}; ///<  starting point point (metric coords)
    double proportion {0}; ///< proportion af affected cells (per event)
    int n_forested {0}; ///< # of pixels forested in all affected regions
    int regions_planned {0}; ///< planned regions (per event)
    int regions_affected {0}; ///< realized # regions
    int n_planned {0}; ///< number of ha planned to be affected (over all regions)
    int n_affected {0}; ///< number of ha affected by the storm (over all regions)
    double mean_susceptiblity {0}; ///< mean susceptibility of all px in all regions
};

/**
 * @brief The WindModule class
 *
 * implements wind disturbance in SVD.
 */
class WindModule : public Module
{
public:
    WindModule(std::string module_name);
    void setup() override;
    std::vector<std::pair<std::string, std::string> > moduleVariableNames() const override;
    double moduleVariable(const Cell *cell, size_t variableIndex) const override;

    void run() override;

    // getters
    const Grid<SWindCell> &windGrid() { return mGrid; }

private:
    // logging
    std::shared_ptr<spdlog::logger> lg;

    /// structure to store scheduled wind events
    struct SWindEvent {
        SWindEvent(int ayear, int id, double ax, double ay, int an_region, double aprop): year(ayear), Id(id), x(ax), y(ay), n_regions(an_region), prop_affected(aprop) {}
       int year; ///< year of storm
       int Id; ///< unique id
       double x, y; ///< coordinates (meter)
       int n_regions; ///< number of 10km regions affected
       double prop_affected; ///<  proportion of cells affected (=severity)
    };

    /// list of all wind events that should be simulated
    std::multimap< int, SWindEvent > mWindEvents;

    /// wind specific grid with stats
    Grid<SWindCell> mGrid;

    Grid<double> mRegionalStormProb; ///< 10km grid with storm probabilities
    int mCellsPerRegion {10000}; ///< number of SVD cells per wind region (10x10km vs 100x100m)

    /// susceptibility to windthrow (depending on state)
    /// based on damage model by Schmidt et al 2010.
    /// values are pre-calculated per state (externally from SVD).
    double getSusceptibility(Cell &c) const;

    /// run a single wind event (which could span multiple regions (10km grid cells))
    void runWindEvent(const SWindEvent &event);

    /// run wind event for a single region (area) and affect (up to) proportion of cells
    SWindStat windImpactOnRegion(const RectF &area, double proportion, const SWindEvent &event);

    /// helper function that samples probabilistically from the container and returns a Point.
    Point sampleFromRegionMap(std::map<Point, double> &map);


    // store for transition probabilites for affected cells
    TransitionMatrix mWindMatrix;

    //double mExtinguishProb {0.}; ///< prob. that a burned pixel stops spreading
    //double mSpreadToDistProb {0.}; ///< the prob. that a fire (with current wind/slope) reaches the neighboring pixel
    Expression mWindSizeMultiplier; ///< scaling factor to change the fire size from the input file
    double mPstopAfterImpact { 0.1 }; ///< probability that impacts stops spreading on a disturbed cell
    double mPspreadUndisturbed { 0.1 }; ///< probability that impacts spread from a undisturbed cell
    double mPfetchFactor { 0.1 }; ///< "fetch factor", i.e. increase in probability for impact on cells adjacent to disturbed cells
    double mStartParallel { 0. }; ///< start the wind spread sequential (0), or more in parallel (>0). When 1, all start points are used immediatel.y
    bool mSaveDebugGrids { false }; ///< save intermediate grids for debugging

    // index of variables
    size_t miDamageProbability{0};

    /// storm statistics for outputs
    std::vector< SWindStat > mStats;

    friend class WindOut;
};

#endif // WINDMODULE_H
