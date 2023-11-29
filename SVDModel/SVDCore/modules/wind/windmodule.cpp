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

WindModule::WindModule(std::string module_name, std::string module_type): Module(module_name, module_type, State::None)
{

}

void WindModule::setup()
{
    lg = spdlog::get("setup");
    lg->info("Setup of WindModule '{}'", name());
    auto settings = Model::instance()->settings();

    settings.requiredKeys("modules." + name(), {"regionalProbabilityGrid", "stormEventFile", "stateFile", "transitionFile",
                                           "stopAfterImpact", "spreadUndisturbed", "fetchFactor", "saveDebugGrids", "windSizeMultiplier", "spreadStartParallel" });


    // set up the transition matrix
    std::string filename = settings.valueString(modkey("transitionFile"));
    mWindMatrix.load(Tools::path(filename));


    // set up additional wind parameter values per state
    filename = settings.valueString(modkey("stateFile"));
    Model::instance()->states()->loadProperties(Tools::path(filename));

    // check if variables are available
    for (const auto *a : {"pWindDamage"})
        if (State::valueIndex(a) == -1)
            throw logic_error_fmt("The WindModule requires the state property '{}' which is not available.", a);

    miDamageProbability = static_cast<size_t>(State::valueIndex("pWindDamage"));

    // set up wind events
    filename = Tools::path(settings.valueString(modkey("stormEventFile")));
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
    mPstopAfterImpact = settings.valueDouble(modkey("stopAfterImpact"));
    mPspreadUndisturbed = settings.valueDouble(modkey("spreadUndisturbed"));
    mPfetchFactor = settings.valueDouble(modkey("fetchFactor"));
    mStartParallel = settings.valueDouble(modkey("spreadStartParallel"));

    mSaveDebugGrids = settings.valueBool(modkey("saveDebugGrids"));

    std::string windsize_multiplier = settings.valueString(modkey("windSizeMultiplier"));
    if (!windsize_multiplier.empty()) {
        mWindSizeMultiplier.setExpression(windsize_multiplier);
        lg->info("windSizeMultiplier is active (value: {}). The maximum number of affected cells will be scaled with this function (variable: total planned cells ).", mWindSizeMultiplier.expression());
    }


    std::string grid_file_name = Tools::path(settings.valueString(modkey("regionalProbabilityGrid")));
    mRegionalStormProb.loadGridFromFile(grid_file_name);
    lg->debug("Loaded regional wind probability grid: '{}'. Dimensions: {} x {}, with cell size: {}m.", grid_file_name, mRegionalStormProb.sizeX(), mRegionalStormProb.sizeY(), mRegionalStormProb.cellsize());


    // setup of the wind grid (values per cell)
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
    mAffectedRects.clear();
    mYearLastExecuted = Model::instance()->year();
    int n_executed=0;
    std::vector<int> skipped_events;
    for (auto i=range.first; i!=range.second; ++i) {
        SWindEvent &event = i->second;

        if (!grid.coordValid(event.x, event.y)) {
            skipped_events.push_back(event.Id);
            continue;
        }

        lg->debug("WindModule: event at {:f}/{:f} with # affected regions: '{}'.", event.x, event.y, event.n_regions);
        double size_multiplier = 1.;
        if (!mWindSizeMultiplier.isEmpty()) {
            double total_predicted = event.n_regions * event.prop_affected * mCellsPerRegion;
            size_multiplier = mWindSizeMultiplier.calculate(total_predicted);
            event.prop_affected = event.prop_affected * size_multiplier;
            lg->debug("Modified wind size from '{}' to '{}' (windSizeMultiplier).", total_predicted, total_predicted*size_multiplier);
        }

        ++n_executed;
        runWindEvent(event);
    }
    if (!skipped_events.empty())
        lg->debug("'{}' events skipped (coordinates out of project area). EventIds: {}",
                  skipped_events.size(),
                  join(skipped_events.begin(), skipped_events.end(), ", "));
    lg->info("WindModule: end of year. #wind events: {}.", n_executed);

    // wind output
    Model::instance()->outputManager()->run("Wind");

}

double WindModule::getSusceptibility(Cell &c) const
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

    SWindStat stat;
    stat.Id = event.Id;
    stat.year = Model::instance()->year();
    stat.x = event.x; stat.y = event.y;
    stat.proportion = event.prop_affected;


    RectF act_area = area.cropped(grid.metricRect());
    if (act_area != area) {
        lg->debug("WindModule: cropping of regional cell (aka 10x10km) to the project area.");
    }
    if (act_area.isNull()) {
        lg->warn("wind-impact: region cell outside of project area.");
        return stat;
    }
    mAffectedRects.push_back(act_area);

    Grid<float> wind_grid(act_area, grid.cellsize());
    wind_grid.initialize(-2.f);
    stat.n_planned = round(stat.proportion * wind_grid.count());

    // create a 1:1 copy of the grid
    Grid<int> wind_debug_grid(act_area, grid.cellsize());
    wind_debug_grid.initialize(0);


    GridRunner<GridCell> runner(grid, act_area);

    std::priority_queue< std::pair<float, Cell*>, std::vector<std::pair<float, Cell*> >, ComparisonClassTopK > queue;

    const size_t n_top = 10 + proportion*1000;

    // (1) look for top k susceptibility values
    // (=potential starting points for spread). Use more starters when proportion is higher=larger impact.
    int n_forested = 0;
    double mean_susceptibility=0.;
    float *grid_ptr = wind_grid.begin();
    while (auto *gr = runner.next()) {
        if (!gr->isNull()) {
            double p_damage = getSusceptibility(gr->cell());
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
    if (n_forested == 0) {
        lg->debug("No forested pixels on tile, exiting.");
        return stat;
    }

    std::stack<Point> start_points_queue; // holds (reversed) starting points
    while (!queue.empty()) {
        PointF loc = grid.cellCenterPoint(queue.top().second->cellIndex());
        // use the *local* coordinates
        start_points_queue.push(wind_grid.indexAt(loc));
        queue.pop();
    }

    // Now add n_starts starting points
    int n_starts = 1 + start_points_queue.size() * mStartParallel;
    while (n_starts > 0 && !start_points_queue.empty()) {
        // add starting point to the queue:
        spread_queue.push(start_points_queue.top());
        start_points_queue.pop();
        --n_starts;
    }
    lg->debug("Event#{}: - Tile: {}. Mean suscetibility on tile: '{}', '{}' forested pixels. Start with {} of {} points.",
              stat.Id, tile_code, mean_susceptibility, n_forested,
              spread_queue.size(), spread_queue.size() + start_points_queue.size());



    // now perform a (conditional) floodfill
    int max_impact = proportion * wind_grid.count(); // assume proportion=prop of total cells
    int n_impact = 0;
    int step = 0;

    // (3) Run the main loop
    // this spreads wind impact
    bool pixel_affected = false;
    while (!spread_queue.empty()) {
        Point p = spread_queue.front();
        spread_queue.pop();

        // when everything is processed, add next starting point
        if (spread_queue.empty()) {
            if (!start_points_queue.empty()) {
                spread_queue.push(start_points_queue.top());
                start_points_queue.pop();
            }
        }

        if (!wind_grid.isIndexValid(p))
            continue;
        float p_local = wind_grid[p];
        if (p_local < 0.f)
            continue;

        if ( drandom() < p_local ) {
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

    } // end main loop


    lg->debug("region finished - Event#: {} Tile: {}. max_impact={}, found={} cells, #processed: {}. length of queue left: {}, number of starting points left: '{}'/'{}'",
              stat.Id, tile_code, max_impact, n_impact, step, spread_queue.size(),
              start_points_queue.size(), n_top);

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

