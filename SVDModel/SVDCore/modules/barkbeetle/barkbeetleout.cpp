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
#include "barkbeetleout.h"
#include "barkbeetlemodule.h"

#include "tools.h"

#include "model.h"
BarkBeetleOut::BarkBeetleOut()
{
    setName("BarkBeetle");
    setDescription("Output on bark beetle activity and grids with the year of the last bark beetle attack on a cell.\n\n" \
                   "Grids are saved as ASCII/TIF grids to the location specified by the" \
                   "`lastBarkBeetleGrid.path` property (`$year$` is replaced with the actual year). " \
                   "The value of the grid cells is the year of the last impact on a cell or 0 for cells never affected by beetles.\n\n" \
                   "### Parameters\n" \
                   " * `lastBarkBeetleGrid.filter`: a grid is written only if the expression evaluates to `true` (with `year` as variable). A value of 0 deactivates the grid output.");
    // define the columns
    columns() = {
                 {"year", "simulation year", DataType::Int},
                 {"n_background_infestation", "number of cells that have a bark beetle infestatation due to random background infesetations (wind-triggered cells are not included)", DataType::Int},
                 {"n_wind_infestation", "number of cells that have a bark beetle infestatation due to wind", DataType::Int},
                 {"n_impact", "number of cells, that are affected by bark beetles (and thus change its state)", DataType::Int},
                 {"n_active_yearend", "number of cells with living bark beetles at the end of the year (i.e., which will be actively spreading next year)", DataType::Int}  };

}


void BarkBeetleOut::setup()
{
    mLastBB.setExpression(Model::instance()->settings().valueString(key("lastBarkBeetleGrid.filter")));
    mLastBBPath = Tools::path(Model::instance()->settings().valueString(key("lastBarkBeetleGrid.path")));
    openOutputFile();

}


void BarkBeetleOut::execute()
{
    BarkBeetleModule *bb_module = dynamic_cast<BarkBeetleModule*>(Model::instance()->moduleByName("BarkBeetle"));
    if (!bb_module)
        return;

    // write output table
    auto &s = bb_module->mStats;
    out() << Model::instance()->year()
          << s.n_background << s.n_wind_infestation
          << s.n_impact
          << s.n_active_yearend;
    out().write();


    // write output grids
    if (mLastBB.isEmpty() || mLastBB.calculateBool( Model::instance()->year() )) {
        std::string file_name = mLastBBPath;
        find_and_replace(file_name, "$year$", to_string(Model::instance()->year()));
        auto &grid = bb_module->mGrid;

        gridToFile<SBeetleCell, short>( grid, file_name, GeoTIFF::DTSINT16,
                                     [](const SBeetleCell &c){ return c.last_attack; });



    }

}

