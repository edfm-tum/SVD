#include "barkbeetlemodule.h"

#include "tools.h"
#include "model.h"
#include "filereader.h"
#include "randomgen.h"
#include "expressionwrapper.h"

#include "../wind/windmodule.h"


BarkBeetleModule::BarkBeetleModule(std::string module_name, std::string module_type): Module(module_name, module_type, State::None)
{

}

void BarkBeetleModule::setup()
{
    lg = spdlog::get("setup");
    lg->info("Setup of BarkBeetleModule '{}'", name());
    auto settings = Model::instance()->settings();

    settings.requiredKeys("modules." + name(), {"climateVarGenerations", "climateVarFrost", "beetleOffspringFactor",
                                                "kernelFile", "successOfColonization", "backgroundProbFormula", "regionalBackgroundProb",
                                                "windInteractionStrength"});


    miVarBBgen = Model::instance()->climate()->indexOfVariable( settings.valueString(modkey("climateVarGenerations")) );
    if (miVarBBgen < 0 )
        throw logic_error_fmt("Barkbeetle: required auxiliary climate variable not found. Looked for (setting climateVarGenerations): '{}'", settings.valueString(modkey("climateVarGenerations")));
    miVarFrost = Model::instance()->climate()->indexOfVariable( settings.valueString(modkey("climateVarFrost")) );
    if (miVarFrost < 0 )
        throw logic_error_fmt("Barkbeetle: required auxiliary climate variable not found. Looked for (setting climateVarFrost): '{}'", settings.valueString(modkey("climateVarFrost")));

    mSuccessOfColonization = settings.valueDouble(modkey("successOfColonization"));
    std::string bb_infest = settings.valueString(modkey("backgroundProbFormula"));
    if (!bb_infest.empty()) {
        mBackgroundProbFormula.setExpression(bb_infest);
        mBackgroundProbVar = mBackgroundProbFormula.addVar("regionalProb");
        lg->info("backgroundProbFormula is active (value: {}). ", mBackgroundProbFormula.expression());
    }

    std::string grid_file_name = Tools::path(settings.valueString(modkey("regionalBackgroundProb")));
    if (!grid_file_name.empty()) {
        mRegionalBackgroundProb.loadGridFromFile(grid_file_name);
        lg->debug("Loaded regional background infestation probability grid: '{}'. Dimensions: {} x {}, with cell size: {}m.", grid_file_name, mRegionalBackgroundProb.sizeX(), mRegionalBackgroundProb.sizeY(), mRegionalBackgroundProb.cellsize());
    }


    mWindInteractionFactor = settings.valueDouble(modkey("windInteractionStrength"));
    int k = settings.valueInt(modkey("beetleOffspringFactor"),-1);
    miSpruce = indexOf(Model::instance()->species(), "piab");
    if (miSpruce < 0)
        throw std::logic_error("Barkbeetle module requires that the species 'piab' is available (see setting model.species).");

    // setup of the kernel
    std::string kernel_file = settings.valueString(modkey("kernelFile"));
    setupKernels(Tools::path(kernel_file), k);

    // set up the transition matrix
    std::string filename = settings.valueString(modkey("transitionFile"));
    mBBMatrix.load(Tools::path(filename));


    // set up additional parameter values for each state state
    filename = settings.valueString(modkey("stateFile"));
    Model::instance()->states()->loadProperties(Tools::path(filename));

    // check if variables are available
    for (const auto *a : {"pBarkBeetleDamage"})
        if (State::valueIndex(a) == -1)
            throw logic_error_fmt("The bark beetle module requires the state property '{}' which is not available.", a);

    miSusceptibility = static_cast<size_t>(State::valueIndex("pBarkBeetleDamage"));
/*    miDamageProbability = static_cast<size_t>(State::valueIndex("pDamage"));

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



    std::string grid_file_name = Tools::path(settings.valueString(modkey("regionalProbabilityGrid")));
    mRegionalStormProb.loadGridFromFile(grid_file_name);
    lg->debug("Loaded regional wind probability grid: '{}'. Dimensions: {} x {}, with cell size: {}m.", grid_file_name, mRegionalStormProb.sizeX(), mRegionalStormProb.sizeY(), mRegionalStormProb.cellsize());
    //lg->info("Metric rectangle with {}x{}m. Left-Right: {:f}m - {:f}m, Top-Bottom: {:f}m - {:f}m.  ", grid.metricRect().width(), grid.metricRect().height(), grid.metricRect().left(), grid.metricRect().right(), grid.metricRect().top(), grid.metricRect().bottom());

*/
    // setup of the grid (values per cell)
    auto &grid = Model::instance()->landscape()->grid();
    mGrid.setup(grid.metricRect(), grid.cellsize());
    lg->debug("Created beetle grid {} x {} cells.", mGrid.sizeX(), mGrid.sizeY());



    lg->info("Setup of BarkBeetleModule '{}' complete.", name());
    lg = spdlog::get("modules");

}

std::vector<std::pair<std::string, std::string> > BarkBeetleModule::moduleVariableNames() const
{
    return {
            {"bbgen", "number of pot. bark beetle generations (Pheinps)"},
            {"frost_days", "number of days with minimum temperture below -15° (from climate data)"},
            {"susceptibility", "stand susceptibility for bark beetle infestations"},
            {"bbNEvents", "cumulative number of times the cell was affected by bark beetle"},
            {"bbLastEvent", "the year the cell was impacted last by bark beetle"},
            {"outbreakAge", "age (years) of the outbreak (the last time the cell was affected)"},
            {"regionalProb", "probability of background infestation"}
    };

}

double BarkBeetleModule::moduleVariable(const Cell *cell, size_t variableIndex) const
{
    if (variableIndex < 2) {
        auto clim_id = cell->environment()->climateId();
        if (variableIndex == 0)
            return Model::instance()->climate()->value(miVarBBgen,clim_id);
        else
            return Model::instance()->climate()->value(miVarFrost,clim_id);
    }
    auto &gr = mGrid[cell->cellIndex()];
    switch (variableIndex) {
    case 2: return cell->state()->value(miSusceptibility);
    case 3: return gr.n_disturbance;
    case 4: return gr.last_attack;
    case 5: return gr.outbreak_age;
    case 6: return backgroundInfestationProb(*cell);

    }

    return 0.;

}

void BarkBeetleModule::run()
{
    // (1) background infestation
    initialRandomInfestation();

    // (2) bark beetle spread and impact
    spread();

    // at the end of the year, reverse the stacks (for next year)
    switchActiveCells();

    // barkbeetle output
    Model::instance()->outputManager()->run("BarkBeetle");
}

void BarkBeetleModule::initialRandomInfestation()
{

    /* idea of subsampling:
     * we pick indices with a fixed distance (subsampling_factor) and a random offset in between.
     * E.g., when subsampling=10, and our random numbers start with [4,6,0,...], our
     * first indices would be: 0+4, 10+6=16, 20+0=20, ...
     * to counteract that we select only a fixed fraction of cells (a different set every year!)
     * the probability for each evaluated pixel increases by the same factor
    */
    CellWrapper cwrap(nullptr);


    auto &cells = Model::instance()->landscape()->cells();

    // subsampling depends on the landscape size
    int subsampling_factor = 1;
    if (cells.size() < 1000000)
        subsampling_factor = 1; // look at all cells (<1Mio)
    else if (cells.size() < 10000000)
        subsampling_factor = 10; // 1-10 Mio forested px
    else
        subsampling_factor = 100; // >10 Mio forested px

    size_t idx = 0;
    int n_started = 0;
    int n_tested = 0;
    size_t max_cell_size = cells.size() - subsampling_factor + 1; // avoid access beyond end of vector

    while (idx < max_cell_size) {
        auto &cell = subsampling_factor>1 ? cells[idx + irandom(0, subsampling_factor)] : cells[idx];
        double susceptibility = cell.state()->value(miSusceptibility);
        // here comes the probability function:
        double p_start = 0.000685;
        if (!mBackgroundProbFormula.isEmpty()) {
            cwrap.setData(&cell);
            double regional_prob = backgroundInfestationProb(cell);
            *mBackgroundProbVar = regional_prob; // make available in the expression
            p_start = mBackgroundProbFormula.calculate(cwrap, regional_prob);
        }
        // effective probability: susceptibility * climate-sensitive prob * subsampling,
        // i.e., when subsampling = 10, then the chance is 10x higher. Since probs are generaly low
        // (< 0.0001 even a factor 100 increase (for perfectly susceptible cells) gives max a 1% chance)
        double p_eff = susceptibility*p_start* subsampling_factor;
        if (susceptibility>0. && drandom() < p_eff ) {
            // start infestation
            auto &g = mGrid[cell.cellIndex()];
            g.outbreak_age = 0; // start again from outbreak age zero
            activeCellsNow().push(cell.cellIndex());
            ++n_started;
        }
        idx += subsampling_factor;
        ++n_tested;
    }


    mStats.n_background = n_started;
    mStats.n_wind_infestation = 0;

    windBeetleInteraction();

    lg->debug("Initial infestation: Checked {} cells (subsampling-factor {}), {} infestations started (started {} wind-interaction cells)",
              n_tested, subsampling_factor, n_started, mStats.n_wind_infestation);
}

void BarkBeetleModule::windBeetleInteraction()
{
    auto *wind_module = dynamic_cast<WindModule*>(Model::instance()->moduleByType("wind"));
    if (!wind_module) {
        lg->debug("No barkbeetle-wind interaction, because there is no active module with the name 'Wind'");
        return;
    }
    int last_wind_year;
    const auto rects = wind_module->affectedRects(last_wind_year);
    if (last_wind_year < 0)
        return;

    short effective_year = static_cast<short>(last_wind_year);

    auto &model_grid = Model::instance()->landscape()->grid();

    const auto &wg = wind_module->windGrid();

    size_t n_started = 0;
    short max_year = -1;
    for (size_t i=0;i < rects.size(); ++i) {
        // loop over each rectangle (10x10km) and scan for wind events this year / last year
        auto runner = GridRunner<SWindCell>(wg,rects[i]);
        while (auto *c = runner.next()) {
            if (c->last_storm>0)
                max_year = std::max(max_year, c->last_storm);
            if (c->last_storm == effective_year) {
                const auto &mcell = model_grid[runner.currentIndex()].cell();
                if (!mcell.state() || mcell.state()->type()==State::None)
                    continue;
                double susceptibility = mcell.state()->speciesProportion()[miSpruce];
                double p_eff = susceptibility*mWindInteractionFactor;
                if (susceptibility>0. && drandom() < p_eff ) {
                    // start infestation
                    auto &g = mGrid[mcell.cellIndex()];
                    g.outbreak_age = 0; // start again from outbreak age zero
                    activeCellsNow().push(mcell.cellIndex());
                    ++n_started;
                }
            }
        }
    }
    mStats.n_wind_infestation = n_started;
}

double BarkBeetleModule::backgroundInfestationProb(const Cell &cell) const
{
    if (mRegionalBackgroundProb.isEmpty())
        return 0.;

     PointF p = mGrid.cellCenterPoint(cell.cellIndex());
     if (!mRegionalBackgroundProb.coordValid(p))
         return 0;

     return mRegionalBackgroundProb(p);
}

void BarkBeetleModule::spread()
{
    const double kernel_value_cap = 10.;

    auto &active_now = activeCellsNow();
    auto &active_next_year = activeCellsNextYear();
    auto *model = Model::instance();

    // variables for book keeping
    size_t n_active_this_year = active_now.size();
    int n_impact = 0.;
    int n_tested = 0;


    while (!active_now.empty()) {
        int cell_index = active_now.top();
        active_now.pop();
        auto &g = mGrid[cell_index];
        auto &cell = model->landscape()->cell(cell_index);
        double generations = model->climate()->value(miVarBBgen, cell.environment()->climateId());
        if (generations == 0.) {
            // if the climate does not support even a single generation,
            // then we do nothing, i.e. not impact happens on this cell.
            continue;
        }
        const auto &kernel = mKernels[kernelIndex(generations)];
        Point cell_point = mGrid.indexOf(cell_index);

        if (g.outbreak_age == 0) {
            // impact on source cell happens only if this is the initial year (i.e. caused by random starts -> outbreak age is still 0)
            g.last_attack = static_cast<short>(model->year());
            g.n_disturbance++;
            ++n_impact;

            // effect of bark beetles: a transition to another state
            state_t new_state = mBBMatrix.transition(cell.stateId());
            cell.setNewState(new_state);
        }

        // (1) spread to neighboring cells using bark beetle kernel
        for (const auto &k : kernel) {
            // get position relative to start point
            Point p = cell_point + k.first;
            if (mGrid.isIndexValid(p)) {
                auto &sgridcell = model->landscape()->grid()[p]; // the location on the grid...
                if (sgridcell.isNull()) // could be empty
                    continue;
                auto &scell = sgridcell.cell(); // ... this is the actual cell
                // susceptibility of the cell depends solely on the state
                // and is modified with a scaling factor
                double susceptibility = scell.state()->value(miSusceptibility);
                susceptibility *= mSuccessOfColonization;

                if (susceptibility == 0.)
                    continue;
                auto &sg = mGrid[p];
                if (sg.last_attack == model->year())
                    continue; // cell has been processed already in this year

                // decide whether beetle should spread to cell 'scell':
                bool do_spread;
                if (k.second > 1.) {
                    // try multiple times: p is 1 - prob not succeding for kernel value times
                    double p_effective = 1. - pow(1. - susceptibility, std::min(k.second, kernel_value_cap) );
                    do_spread = drandom() < p_effective;

                } else {
                    // kernel value is probability of successful spread with unlimited susceptibility
                    // we scale here the probability with susceptibility
                    double p_spread = susceptibility*k.second;
                    do_spread = drandom() < p_spread;
                }
                ++n_tested;

                if (do_spread) {

                    int scell_index = mGrid.index(p);

                    // impact of cell
                    sg.last_attack = static_cast<short>(model->year());
                    sg.n_disturbance++;
                    ++n_impact;

                    // effect of bark beetles: a transition to another state
                    state_t new_state = mBBMatrix.transition(scell.stateId());
                    scell.setNewState(new_state);
                    sg.outbreak_age = g.outbreak_age + 1; // increase age of outbreak from the source cell

                    // mortality of cell: following the approach of iLand here
                    double frost_days = model->climate()->value(miVarFrost, cell.environment()->climateId());
                    const double base_mortality = 0.4; // fixed proportion of cells dying (based on Jönsson 2012)
                    double frost_mortality = 1. - exp( -0.1005 * frost_days); // probability of mortality due to strong frost (Kostal et al 2011)
                    // p of either base or frost mortality:
                    double p_mort = base_mortality + frost_mortality - (base_mortality * frost_mortality);

                    // mortality due to age of the outbreak: 50% in year 5, 100% later.
                    // value is 1 for 4 yrs, 0.5 for 5yrs, 0 for 6 yrs and above
                    if (sg.outbreak_age > 4) {
                        double wave_survival = 1. - ( sg.outbreak_age < 5 ? 0. : std::min( (sg.outbreak_age - 4)/2., 1.) );
                        // effective mortality with reduced survival rate
                        p_mort = 1. - ( 1. - p_mort)*wave_survival;
                    }
                    if (drandom() < p_mort) {
                        // mortality
                        // nothing to do?
                        sg.outbreak_age = 0; // reset
                    } else {
                        // cell survives: is active next year and can spread
                        active_next_year.push(scell_index);
                    }
                }

            }
        }

    }
    mStats.n_active_yearend = active_next_year.size();
    mStats.n_impact = n_impact;
    lg->debug("Barkbeetle spread. Active before spread: {}, #cell tested (cells with susceptibility>0): {}, #cells spread to and impacted: {}, surviving beetle cells active next year: {} ",
              n_active_this_year, n_tested, n_impact, active_next_year.size());



}

void BarkBeetleModule::setupKernels(const std::string &file_name,  int offspring_factor)
{

    FileReader rdr(file_name);

    if (rdr.columnCount() != 123)
        throw logic_error_fmt("Invalid kernel data file for bark beetle!");

    mKernels.clear();
    mKernels.resize(5); // 1, 1.5, 2, 2.5, 3

    size_t i_gen = rdr.columnIndex("vgen");
    size_t i_k = rdr.columnIndex("vk");
    while (rdr.next()) {
        if (rdr.value(i_k) == offspring_factor) {
            size_t ki = kernelIndex(rdr.value(i_gen));
            for (int i=2; i<123; ++i) {
                int d_x = (i-2) % 11 - 5;
                int d_y = ((i-2) / 11) - 5;
                double value = rdr.value(i);
                if (value > 0.) {
                    //lg->debug("{}: x: {} y: {}, value: {}", i, d_x, d_y, value);
                    mKernels[ki].push_back(std::pair<Point, double>( Point(d_x, d_y), value  ));
                }
            }
        }
    }
    // test
    for (size_t i=0; i<mKernels.size(); ++i) {
        if (mKernels[i].empty())
            throw logic_error_fmt("setup barkbeetle kernels: no data available for kernel {}", i);
    }
    lg->debug("Barkbeetle kernels loaded from '{}'", file_name);

}

size_t BarkBeetleModule::kernelIndex(double gen_count) const
{
    if (gen_count == 1.) return 0;
    if (gen_count == 1.5) return 1;
    if (gen_count == 2.) return 2;
    if (gen_count == 2.5) return 3;
    if (gen_count == 3.) return 4;
    throw logic_error_fmt("barkbeetle: invalid number of generations: '{}' (allowed are: 1,1.5,2,2.5,3)", gen_count);

}
