#include "barkbeetlemodule.h"

#include "tools.h"
#include "model.h"
#include "filereader.h"
#include "randomgen.h"


BarkBeetleModule::BarkBeetleModule(std::string module_name): Module(module_name, State::None)
{

}

void BarkBeetleModule::setup()
{
    lg = spdlog::get("setup");
    lg->info("Setup of BarkBeetleModule '{}'", name());
    auto settings = Model::instance()->settings();

    settings.requiredKeys("modules." + name(), {"climateVarGenerations", "climateVarFrost"  });


    miVarBBgen = Model::instance()->climate()->indexOfVariable( settings.valueString(modkey("climateVarGenerations")) );
    if (miVarBBgen < 0 )
        throw logic_error_fmt("Barkbeetle: required auxiliary climate variable not found. Looked for (setting climateVarGenerations): '{}'", settings.valueString(modkey("climateVarGenerations")));
    miVarFrost = Model::instance()->climate()->indexOfVariable( settings.valueString(modkey("climateVarFrost")) );
    if (miVarFrost < 0 )
        throw logic_error_fmt("Barkbeetle: required auxiliary climate variable not found. Looked for (setting climateVarFrost): '{}'", settings.valueString(modkey("climateVarFrost")));


/*
    // set up the transition matrix
    std::string filename = settings.valueString(modkey("transitionFile"));
    mWindMatrix.load(Tools::path(filename));


    // set up additional wind parameter values per state
    filename = settings.valueString(modkey("stateFile"));
    Model::instance()->states()->loadProperties(Tools::path(filename));

    // check if variables are available
    for (const auto *a : {"pDamage"})
        if (State::valueIndex(a) == -1)
            throw logic_error_fmt("The WindModule requires the state property '{}' which is not available.", a);

    miDamageProbability = static_cast<size_t>(State::valueIndex("pDamage"));

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
    //lg->info("Metric rectangle with {}x{}m. Left-Right: {:f}m - {:f}m, Top-Bottom: {:f}m - {:f}m.  ", grid.metricRect().width(), grid.metricRect().height(), grid.metricRect().left(), grid.metricRect().right(), grid.metricRect().top(), grid.metricRect().bottom());


    // setup of the wind grid (values per cell)
    auto &grid = Model::instance()->landscape()->grid();
    mGrid.setup(grid.metricRect(), grid.cellsize());
    mCellsPerRegion = (mRegionalStormProb.cellsize() / grid.cellsize())*(mRegionalStormProb.cellsize() / grid.cellsize());
    lg->debug("Created wind grid {} x {} cells. CellsPerRegion: {}.", mGrid.sizeX(), mGrid.sizeY(), mCellsPerRegion);

*/

    lg->info("Setup of BarkBeetleModule '{}' complete.", name());

    lg = spdlog::get("modules");

}

std::vector<std::pair<std::string, std::string> > BarkBeetleModule::moduleVariableNames() const
{
    return {
            {"bbgen", "number of pot. bark beetle generations (Pheinps)"},
            {"wintermin", "number of days with minimum temperture below threshold (from climate data)"}
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
    return 0.;



    /*
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
    } */

}

void BarkBeetleModule::run()
{
    // barkbeetle output
    // Model::instance()->outputManager()->run("BarkBeetle");
}
