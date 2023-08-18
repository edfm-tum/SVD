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
#include "windout.h"
#include "windmodule.h"

#include "tools.h"

#include "model.h"
WindOut::WindOut()
{
    setName("Wind");
    setDescription("Output on wind events (one storm per line) and grids for the year of the last burn.\n\n" \
                   "Grids are saved as ASCII/TIF grids to the location specified by the" \
                   "`lastFireGrid.path` property (`$year$` is replaced with the actual year). " \
                   "The value of the grid cells is the year of the last burn in a cell or 0 for unburned cells.\n\n" \
                   "### Parameters\n" \
                   " * `lastFireGrid.filter`: a grid is written only if the expression evaluates to `true` (with `year` as variable). A value of 0 deactivates the grid output.");
    // define the columns
    columns() = {
                 {"year", "simulation year of the storm event", DataType::Int},
                 {"id", "unique identifier of a single storm event", DataType::Int},
                 {"x", "x coordinate (m) of the centerpoint of the initial regional grid cell", DataType::Double},
                 {"y", "y coordinate (m) of the centerpoint of the initial regional grid cell", DataType::Double},
                 {"proportion", "(predefined) proportion of affected cells (per event)", DataType::Double},
                 {"regions_planned", "# of regions planned to be affected", DataType::Int},
                 {"regions_affected", "realized number of regions (10x10km) by the storm event", DataType::Int},
                 {"cells_forested", "total number of forested cells (ha) in all regions", DataType::Int},
                 {"mean_susceptibility", "mean wind susceptibility of forested cells (ha) in all regions", DataType::Double},
                 {"cells_planned", "number of ha planned to be affected (over all regions)", DataType::Double},
                 {"cells_affected", "number of ha affected by the storm (over all regions)", DataType::Double}   };

}


void WindOut::setup()
{
    mLastWind.setExpression(Model::instance()->settings().valueString(key("lastWindGrid.filter")));
    mLastWindPath = Tools::path(Model::instance()->settings().valueString(key("lastWindGrid.path")));
    openOutputFile();

}


void WindOut::execute()
{
    WindModule *wind = dynamic_cast<WindModule*>(Model::instance()->module("wind"));
    if (!wind)
        return;

    // write output table
    for (auto &s : wind->mStats) {
        if (s.year == Model::instance()->year()) {
            out() << s.year << s.Id << s.x << s.y <<
                s.proportion << s.regions_planned << s.regions_affected <<
                s.n_forested << s.mean_susceptiblity <<
                s.n_planned << s.n_affected;
            out().write();
        }
    }

    // write output grids
    if (mLastWind.isEmpty() || mLastWind.calculateBool( Model::instance()->year() )) {
        std::string file_name = mLastWindPath;
        find_and_replace(file_name, "$year$", to_string(Model::instance()->year()));
        auto &grid = wind->mGrid;

        gridToFile<SWindCell, short>( grid, file_name, GeoTIFF::DTSINT16,
                                     [](const SWindCell &c){ return c.n_storm; });



    }

}

