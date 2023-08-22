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
#include "windmodule.h"

#include "tools.h"
#include "model.h"
#include "filereader.h"
#include "randomgen.h"

#include <queue>
#include <stack>

#ifndef M_PI
#define M_PI 3.141592653589793
#endif

WindModule::WindModule(std::string module_name): Module(module_name, State::None)
{

}

void WindModule::setup()
{
    lg = spdlog::get("setup");
    lg->info("Setup of WindModule '{}'", name());
    auto settings = Model::instance()->settings();

    settings.requiredKeys("modules.wind", {"regionalProbabilityGrid", "stormEventFile", "stateFile", "transitionFile",
                                           "stopAfterImpact", "spreadUndisturbed", "fetchFactor", "saveDebugGrids", "windSizeMultiplier" });


    // set up the transition matrix
    std::string filename = settings.valueString("modules.wind.transitionFile");
    mWindMatrix.load(Tools::path(filename));


    // set up additional fire parameter values per state
    filename = settings.valueString("modules.wind.stateFile");
    Model::instance()->states()->loadProperties(Tools::path(filename));

    // check if variables are available
    for (const auto *a : {"pDamage"})
        if (State::valueIndex(a) == -1)
            throw logic_error_fmt("The WindModule requires the state property '{}' which is not available.", a);

    miDamageProbability = static_cast<size_t>(State::valueIndex("pDamage"));
/*
    // check if DEM is available
    if (settings.valueString("visualization.dem").empty())
        throw logic_error_fmt("The fire module requires a digital elevation model! {}", 0);
*/

    // set up wind events
    filename = Tools::path(settings.valueString("modules.wind.stormEventFile"));
    FileReader rdr(filename);
    rdr.requiredColumns({"year", "x", "y", "number_of_cells", "proportion_of_cell"});
    size_t iyr=rdr.columnIndex("year"), ix=rdr.columnIndex("x"), iy=rdr.columnIndex("y"), in_cell=rdr.columnIndex("number_of_cells"), iprop=rdr.columnIndex("proportion_of_cell");
    size_t iid = rdr.columnIndex("id");
    int storm_id = 0;
    while (rdr.next()) {
        int year = static_cast<int>(rdr.value(iyr));
        storm_id = iid!=std::string::npos ? static_cast<int>(rdr.value(iid)) : storm_id + 1; // use provided Ids or a custom Id
        mWindEvents.emplace(std::make_pair(year, SWindEvent(year, storm_id, rdr.value(ix), rdr.value(iy),
                                                           rdr.value(in_cell), rdr.value(iprop))));
    }
    lg->debug("Loaded {} storm events from '{}'", mWindEvents.size(), filename);

    // setup parameters
    mPstopAfterImpact = settings.valueDouble("modules.wind.stopAfterImpact");
    mPspreadUndisturbed = settings.valueDouble("modules.wind.spreadUndisturbed");
    mPfetchFactor = settings.valueDouble("modules.wind.fetchFactor");
    mSaveDebugGrids = settings.valueBool("modules.wind.saveDebugGrids");

    std::string windsize_multiplier = settings.valueString("modules.wind.windSizeMultiplier");
    if (!windsize_multiplier.empty()) {
        mWindSizeMultiplier.setExpression(windsize_multiplier);
        lg->info("windSizeMultiplier is active (value: {}). The maximum number of affected cells will be scaled with this function (variable: total planned cells ).", mWindSizeMultiplier.expression());
    }


    /*

    // set up parameters

    mExtinguishProb = settings.valueDouble("modules.fire.extinguishProb");
    mSpreadToDistProb = 1. - settings.valueDouble("modules.fire.spreadDistProb");
    std::string firesize_multiplier = settings.valueString("modules.fire.fireSizeMultiplier");
    if (!firesize_multiplier.empty()) {
        mWindSizeMultiplier.setExpression(firesize_multiplier);
        lg->info("fireSizeMultiplier is active (value: {}). The maximum fire size of fires will be scaled with this function (variable: max fire size (ha)).", mWindSizeMultiplier.expression());
    }
    */
    std::string grid_file_name = Tools::path(settings.valueString("modules.wind.regionalProbabilityGrid"));
    mRegionalStormProb.loadGridFromFile(grid_file_name);
    lg->debug("Loaded regional wind probability grid: '{}'. Dimensions: {} x {}, with cell size: {}m.", grid_file_name, mRegionalStormProb.sizeX(), mRegionalStormProb.sizeY(), mRegionalStormProb.cellsize());
    //lg->info("Metric rectangle with {}x{}m. Left-Right: {:f}m - {:f}m, Top-Bottom: {:f}m - {:f}m.  ", grid.metricRect().width(), grid.metricRect().height(), grid.metricRect().left(), grid.metricRect().right(), grid.metricRect().top(), grid.metricRect().bottom());


    // setup of the fire grid (values per cell)
    auto &grid = Model::instance()->landscape()->grid();
    mGrid.setup(grid.metricRect(), grid.cellsize());
    mCellsPerRegion = (mRegionalStormProb.cellsize() / grid.cellsize())*(mRegionalStormProb.cellsize() / grid.cellsize());
    lg->debug("Created wind grid {} x {} cells. CellsPerRegion: {}.", mGrid.sizeX(), mGrid.sizeY(), mCellsPerRegion);

    lg->info("Setup of WindModule '{}' complete.", name());

    lg = spdlog::get("modules");

}

std::vector<std::pair<std::string, std::string> > WindModule::moduleVariableNames() const
{
    return {{"regionalProb", "wind event probability at regional scale (10km)"},
        {"susceptibility", "state-dependent probability of wind damage"},
        {"windNEvents", "cumulative number of wind events on cell"},
        {"windLastEvent", "the year of the last storm event on cell (or 0 if never affected)"}};
}

double WindModule::moduleVariable(const Cell *cell, size_t variableIndex) const
{
    auto &gr = mGrid[cell->cellIndex()];
    switch (variableIndex) {
    case 0: {
        // 10km grid
        return mRegionalStormProb.constValueAt(mGrid.cellCenterPoint(cell->cellIndex()));
    }
    case 1: // susceptibility
        return cell->state()->value(miDamageProbability);
    case 2:
        return gr.n_storm;
    case 3:
        return gr.last_storm;
    default:
        return 0.;
    }
}

static const Point eight_neighbors[8] = {Point(-1,0), Point(1,0), Point(0,-1), Point(-1,0),
                                         Point(-1,-1), Point(-1,1), Point(1,-1), Point(1,1)};


void WindModule::run()
{
    // check if we have events
    auto &grid = Model::instance()->landscape()->grid();
    auto range = mWindEvents.equal_range(Model::instance()->year());
    int n_executed=0;
    for (auto i=range.first; i!=range.second; ++i) {
        SWindEvent &event = i->second;
        lg->debug("WindModule: event at {:f}/{:f} with # affected regions: '{}'.", event.x, event.y, event.n_regions);
        if (!grid.coordValid(event.x, event.y)) {
            lg->debug("Coordinates out of project area. Skipping.");
            continue;
        }
        double size_multiplier = 1.;
        if (!mWindSizeMultiplier.isEmpty()) {
            double total_predicted = event.n_regions * event.prop_affected * mCellsPerRegion;
            size_multiplier = mWindSizeMultiplier.calculate(total_predicted);
            event.prop_affected = event.prop_affected * size_multiplier;
            lg->debug("Modified fire size from '{}' to '{}' (windSizeMultiplier).", total_predicted, total_predicted*size_multiplier);
        }

        ++n_executed;
        runWindEvent(event);
    }
    lg->info("WindModule: end of year. #wind events: {}.", n_executed);

    // wind output
    Model::instance()->outputManager()->run("Wind");

}

double WindModule::calculateSusceptibility(Cell &c) const
{
    /// pre-calculated value, stored as extra column
    double pDamage = c.state()->value(miDamageProbability);
    return pDamage;
}

Point WindModule::sampleFromRegionMap(std::map<Point, double> &map)
{
    if (map.empty())
        return Point(-1, -1); // invalid location

    // calculate the sum of probs
    double p_sum = 0.;
    for (auto it=map.begin(); it!=map.end(); ++it)
        p_sum += it->second;

    double p = nrandom(0., p_sum);

    p_sum = 0.;
    for (auto it=map.begin(); it!=map.end(); ++it) {
        p_sum += it->second;
        if (p < p_sum)
            return it->first;
    }
    return map.rbegin()->first;
}


void WindModule::runWindEvent(const SWindEvent &event)
{
    // spread to multiple cells....
    auto p=mRegionalStormProb.indexAt(PointF(event.x, event.y));

    std::map<Point, double> region_list;
    std::vector<SWindStat> stats;
    region_list[p] = 1.; // initial cell (will be returned by initial sampling)

    int regions_to_process = event.n_regions;
    int processed = 0;
    while (regions_to_process > 0) {

        Point p_sampled = sampleFromRegionMap(region_list);
        if (p_sampled.x() < 0)
            break;

        RectF cell_rect = mRegionalStormProb.cellRect(p_sampled);

        // call wind routine on regional cell
        stats.push_back(windImpactOnRegion(cell_rect, event.prop_affected, event));

        ++processed;
        --regions_to_process;
        region_list[p_sampled] = 0.;

        // add further candidate regions
        for (int i=0;i<8;++i) {
            Point pd = p_sampled + eight_neighbors[i];
            if (!mRegionalStormProb.isIndexValid(pd))
                 continue; // not in probability map
            if (!mGrid.coordValid(mRegionalStormProb.cellCenterPoint(pd)))
                 continue; // not in project area
            if ( region_list.find(pd) != region_list.end())
                 continue; // already in list

            // add to list
            region_list[pd] = mRegionalStormProb[pd];
        }


    }
    if (!stats.empty()) {
    // process/aggregate stats: take last element....
        SWindStat cstat =stats.back();
        stats.pop_back();
        //... and cumulatively add the others
        double msus = cstat.mean_susceptiblity * cstat.n_forested;
        for (auto &s : stats) {
            cstat.n_affected += s.n_affected;
            cstat.n_forested += s.n_forested;
            cstat.n_planned += s.n_planned;
            msus += s.mean_susceptiblity * s.n_forested;

        }
        cstat.mean_susceptiblity = msus / cstat.n_forested; // weighed with forested pixels
        cstat.regions_affected = processed;
        cstat.regions_planned = event.n_regions;
        mStats.push_back(cstat);
    }

    lg->debug("End of wind-event #{}: Regions planned: '{}', #Regions affected: {} (Regions not processed: {}). cells affected/planned: '{}'/'{}', mean susceptibility: '{}'",
              event.Id, event.n_regions, processed, regions_to_process,
              mStats.back().n_affected, mStats.back().n_planned, mStats.back().mean_susceptiblity);

}

class ComparisonClassTopK {
public:
    bool operator() (const std::pair<float, Cell*> &p1, const std::pair<float, Cell*> &p2) {
        //comparison code here: we want to keep track of the largest element
        return p1.first>p2.first;
    }
};


SWindStat WindModule::windImpactOnRegion(const RectF &area, double proportion, const SWindEvent &event)
{
    auto &grid = Model::instance()->landscape()->grid();
    Grid<float> wind_grid(area, grid.cellsize());
    wind_grid.initialize(-2.f);
    // create a 1:1 copy of the grid
    Grid<int> wind_debug_grid(area, grid.cellsize());
    wind_debug_grid.initialize(0);

    GridRunner<GridCell> runner(grid, area);

    std::priority_queue< std::pair<float, Cell*>, std::vector<std::pair<float, Cell*> >, ComparisonClassTopK > queue;

    SWindStat stat;
    stat.Id = event.Id;
    stat.year = Model::instance()->year();
    stat.x = event.x; stat.y = event.y;
    stat.proportion = event.prop_affected;
    stat.n_planned = stat.proportion * wind_grid.count();


    const size_t n_top = 10 + proportion*1000;
    const double temperature = 0.1; // 0..1; 0: strictly as provided, 1

    // look for top k susceptibility values
    int n_forested = 0;
    double mean_susceptibility=0.;
    float *grid_ptr = wind_grid.begin();
    while (auto *gr = runner.next()) {
        if (!gr->isNull()) {
            double p_damage = calculateSusceptibility(gr->cell());
            *grid_ptr = p_damage; // write susceptibility values to wind grid
            mean_susceptibility += p_damage;
            ++n_forested;

            if (queue.size()<n_top || p_damage > queue.top().first) {
                if (queue.size() == n_top)
                    queue.pop();
                queue.push( std::pair<float, Cell*>(p_damage, &gr->cell()) );
            }
        }
        ++grid_ptr;
    }
    mean_susceptibility /= n_forested > 0? n_forested : 1;

    std::string tile_code = std::to_string(area.left()) + "_" + std::to_string(area.top());
    if (mSaveDebugGrids) {
        std::string filename = Tools::path("temp/suscept_tile_" + tile_code + "_" + std::to_string(Model::instance()->year()) + ".asc" );
        std::string result = gridToESRIRaster<float>(wind_grid);
        if (!writeFile(filename, result))
            throw std::logic_error("Wind: couldn't write debug output grid file: " + filename);
    }


    std::queue<Point> spread_queue;
    lg->debug("Event#{}: - Tile: {}. Mean suscetibility on tile: '{}', '{}' forested pixels.", stat.Id, tile_code, mean_susceptibility, n_forested);
    //lg->debug("Number of starting positions: '{}'. ", n_top);

    std::stack<Point> reverse_queue; // helper to reverse the order of elements
    while (!queue.empty()) {
        //lg->debug("Cell: {}, p={}", queue.top().second->cellIndex(), queue.top().first);
        // queue.top().second->setNewState(6); // ABAL 2-4m - just a test

        // add values as starting points
        PointF loc = grid.cellCenterPoint(queue.top().second->cellIndex());
        // use the *local* coordinates
        reverse_queue.push(wind_grid.indexAt(loc));
        queue.pop();
    }
    // we reverse the order, as we want to start with the pixel with *highest* susceptibility
//    while (!reverse_queue.empty()) {
//        spread_queue.push(reverse_queue.top());
//        reverse_queue.pop();
//    }
    // add a single starting point to the queue:
    spread_queue.push(reverse_queue.top());
    reverse_queue.pop();



    // now perform a (conditional) floodfill
    int max_impact = proportion * wind_grid.count(); // assume proportion=prop of total cells
    int n_impact = 0;
    int step = 0;


    bool pixel_affected = false;
    while (!spread_queue.empty()) {
        Point p = spread_queue.front();
        spread_queue.pop();

        // when everything is processed, add next starting pont
        if (spread_queue.empty()) {
            if (!reverse_queue.empty()) {
                spread_queue.push(reverse_queue.top());
                reverse_queue.pop();
            }
        }

        if (!wind_grid.isIndexValid(p))
            continue;
        float p_local = wind_grid[p];
        if (p_local < 0.f)
            continue;

        if ( drandom() > p_local ) {
            // (i) impact the cell
            // ====================
            wind_grid[p] = -1.; // mark cell as already processed
            PointF loc = wind_grid.cellCenterPoint(p);
            auto &s = grid[loc].cell();

            // effect of wind: a transition to another state
            state_t new_state = mWindMatrix.transition(s.stateId());
            s.setNewState(new_state);

            auto &stat = mGrid[loc];
            stat.last_storm = Model::instance()->year();
            ++stat.n_storm;
            ++n_impact;
            pixel_affected = true;
            wind_debug_grid[p] = 10000 + step;
        } else {
            // not affected
            wind_grid[p] = -0.5; // mark as processed (but not affected)
            wind_debug_grid[p] = step;
            pixel_affected = false;
        }
        if (pixel_affected || drandom() < mPspreadUndisturbed) {
            // now check neigbors
            for (int i=0;i<8;++i) {
                // increase probability of neighboring pixels
                Point pd = p + eight_neighbors[i];
                if (wind_grid.isIndexValid(pd)) {
                    if (wind_grid[pd] < 0.) // already processed
                        continue;
                    wind_grid[pd] += mPfetchFactor; // add a prob for neighbors
                    // there is a chance that wind-impact does not spread further
                    if (mPstopAfterImpact>0. && drandom() < mPstopAfterImpact)
                        continue;
                    // add to queue *only* if impact does not stop here
                    spread_queue.push(pd);
                }
            }
        }
        ++step;
        if (n_impact >= max_impact) {
            // we stop here, enough pixel found!
            break;
        }

    }
    lg->debug("region finished - Event#: {} Tile: {}. max_impact={}, found={} cells, #processed: {}. length of queue left: {}, number of starting points left: '{}'/'{}'",
              stat.Id, tile_code, max_impact, n_impact, step, spread_queue.size(),
              reverse_queue.size(), n_top);

    // do some stats, saves
    //mRegionalStormProb.indexOf()
    if (mSaveDebugGrids) {
        std::string filename = Tools::path("temp/damage_tile_" + tile_code + "_" + std::to_string(Model::instance()->year()) + ".asc" );
        std::string result = gridToESRIRaster<int>(wind_debug_grid);
        if (!writeFile(filename, result))
            throw std::logic_error("FireOut: couldn't write output grid file: " + filename);
    }

    stat.mean_susceptiblity = mean_susceptibility;
    stat.n_forested = n_forested;
    stat.n_affected = n_impact;

    return stat;
}

