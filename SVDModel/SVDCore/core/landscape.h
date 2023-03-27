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
#ifndef LANDSCAPE_H
#define LANDSCAPE_H

#include <cassert>

#include "grid.h"
#include "cell.h"
#include "environmentcell.h"

/// GridCell is a light-weight proxy (4 bytes)
/// for convenient access to Cell values
struct GridCell {
    GridCell(): index(-1) {}
    bool isNull() const { return index < 0; }
    /// get a ref to the cell
    Cell &cell() const { assert(index>=0 && static_cast<size_t>(index)<(*mCellVector).size());
                   return (*mCellVector)[index];
                 }
    Cell & operator->() const { return cell(); }
    int index;

    // to access the cells of the parent object
    static std::vector<Cell> *mCellVector;
};

class Landscape
{
public:
    Landscape();
    void setup();


    // access
    // access single cells
    Cell &cell(int index) { assert(mGrid.isIndexValid(index));
                            return mGrid[index].cell();  }
    EnvironmentCell &environmentCell(int index) { assert(mEnvironmentGrid.isIndexValid(index));
                                                assert(mEnvironmentGrid[index] != nullptr);
                                                return *mEnvironmentGrid[index]; }

    /// number of active cells (forested)
    int NCells() const { return mNCells; }

    // access whole grids


    /// get the spatial. Cells outside the project area are marked
    /// by cells where isNull() is true.
    Grid<GridCell> &grid() { return mGrid; }

    /// access the actual grid cells
    /// the vector contains all valid cells on the landscape
    std::vector<Cell> &cells() { return mCells; }
    /// environment-grid: pointer to EnvironmentCell, nullptr if invalid.
    Grid<EnvironmentCell*> &environment()  { return mEnvironmentGrid; }

    /// a list of all climate ids (regions) that are present in the current landscape
    const std::map<int, int> &climateIds() { return mClimateIds; }

    /// get elevation of given cell with cellIndex() index
    float elevationOf(const int index) { if(mDEM.isEmpty()) return 0.F; return mDEM.valueAt( mGrid.cellCenterPoint( index ) ); }

    /// get elevation of given cell with cellIndex() index
    float elevationAt(const PointF &coord) { if(mDEM.isEmpty()) return 0.F; return mDEM.valueAt( coord ); }
private:
    void setupInitialState();
    // Grid<Cell> mGrid; ///< main container for the landscape
    Grid<GridCell> mGrid; ///< spatial grid, stores indices to mCells
    std::vector<Cell> mCells; ///< container for cells

    Grid<EnvironmentCell*> mEnvironmentGrid; ///< the grid covers the full landscape, and each value points to a cell with the actual env. values
    std::vector<EnvironmentCell> mEnvironmentCells; ///< each EnvironmentCell defines a region
    int mNCells; ///< number of valid cells on the landsscape

    std::map<int, int> mClimateIds;

    /// digital elevation model
    Grid<float> mDEM;
};

#endif // LANDSCAPE_H
