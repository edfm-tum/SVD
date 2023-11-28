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
#ifndef FIREMODULE_H
#define FIREMODULE_H

#include <map>

#include "modules/module.h"

#include "transitionmatrix.h"
#include "states.h"
#include "grid.h"
#include "spdlog/spdlog.h"
#include "expression.h"

class FireOut; // forward

struct SFireCell {
    SFireCell() = default;
    float spread { 0.F }; ///< spread flag during current fire event
    short int n_fire { 0 }; ///< counter how often cell burned
    short int n_high_severity {0}; ///< high severity counter
    short int last_burn {0}; ///< year when the cell burned the last time
};

struct SFireStat {
    int year; ///< year of the fire
    int Id; ///< unique identifier
    double x, y; ///<  ignition point (metric coords)
    int max_size; ///< maximum fire size (ha)
    int ha_burned; ///< # of ha. burned
    int ha_high_severity; ///< # of ha burned with high severity
    Rect fire_bounding_box; ///< bounding box in cell coordinates

};

class FireModule : public Module
{
public:
    FireModule(std::string module_name, std::string module_type);
    void setup() override;
    std::vector<std::pair<std::string, std::string> > moduleVariableNames() const override;
    double moduleVariable(const Cell *cell, size_t variableIndex) const override;

    void run() override;

    // getters
    const Grid<SFireCell> &fireGrid() { return mGrid; }

private:

    // logging
    std::shared_ptr<spdlog::logger> lg;


    // store for ignitions
    struct SIgnition {
        SIgnition(int ayear, int id, double ax, double ay, double amax, double wspeed, double wdirection): year(ayear), Id(id), x(ax), y(ay), max_size(amax), wind_speed(wspeed), wind_direction(wdirection) {}
       int year;
       int Id; // unique id
       double x, y; // cooridnates (meter)
       double max_size; // maximum fire size in ha
       double wind_speed; // current wind speed (m/2)
       double wind_direction; // wind direction in degrees (0=north, 90=east, ...)
    };
    std::multimap< int, SIgnition > mIgnitions;

    Grid<SFireCell> mGrid;

    void fireSpread(const SIgnition &ign);
    bool burnCell(int ix, int iy, int &rHighSeverity, int round);

    double calcSlopeFactor(const double slope) const;
    double calcWindFactor(const SIgnition &fire_event, const double direction) const;
    void calculateSpreadProbability(const SIgnition &fire_event, const Point &point, const float origin_elevation,  const int direction);


    // store for transition probabilites for burned cells
    TransitionMatrix mFireMatrix;

    double mExtinguishProb {0.}; ///< prob. that a burned pixel stops spreading
    double mSpreadToDistProb {0.}; ///< the prob. that a fire (with current wind/slope) reaches the neighboring pixel
    Expression mFireSizeMultiplier; ///< scaling factor to change the fire size from the input file

    // index of variables
    size_t miBurnProbability{0};
    size_t miHighSeverity{0};

    // fire statistics
    std::vector< SFireStat > mStats;

    friend class FireOut;
};

#endif // FIREMODULE_H
