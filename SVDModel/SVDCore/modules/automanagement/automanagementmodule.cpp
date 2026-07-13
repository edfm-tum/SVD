#include "automanagementmodule.h"

#include "model.h"
#include "environmentcell.h"
#include "tools.h"
#include "filereader.h"

AutoManagementModule::AutoManagementModule(std::string module_name, std::string module_type): Module(module_name, module_type, State::None)
{

}

void AutoManagementModule::setup()
{
    lg = spdlog::get("setup");
    lg->info("Setup of AutoManagementModule '{}'", name());
    auto settings = Model::instance()->settings();
    settings.requiredKeys("modules." + name(), {"stateFile", "burnInProbability", "transitionFile", "managementCapGrid", "managementCapModifier" });

    // set up the transition matrix
    std::string filename = settings.valueString(modkey("transitionFile"));
    mMgmtMatrix.load(Tools::path(filename));

    // set up additional parameter values per state (height increment threshold)
    filename = Tools::path(settings.valueString(modkey("stateFile")));
    Model::instance()->states()->loadProperties(filename);
    // check if variables are available
    for (auto a : {"heightIncrementThreshold", "pManagement"})
        if (State::valueIndex(a) == -1)
            throw logic_error_fmt("AutoManagement requires the state property '{}' which is not available.", a);

    miIncrementThreshold = static_cast<size_t>(State::valueIndex("heightIncrementThreshold"));
    miManagement = static_cast<size_t>(State::valueIndex("pManagement"));

    // management caps
    std::string grid_file_name = Tools::path(settings.valueString(modkey("managementCapGrid")));
    if (!grid_file_name.empty()) {
        mManagementCapGrid.loadGridFromFile(grid_file_name);
        mManagementCapModifier = settings.valueDouble(modkey("managementCapModifier"), 1.);
        lg->debug("Loaded management cap grid: '{}'. Dimensions: {} x {}, with cell size: {}m. managementCapModifier={}% ",
              grid_file_name, mManagementCapGrid.sizeX(), mManagementCapGrid.sizeY(),
              mManagementCapGrid.cellsize(), mManagementCapModifier*100.);
    } else {
        lg->debug("ManagementCap is not active (no grid provided), only bottom up management.");
    }

    // equation for burn-in phase of management
    std::string burninprob = settings.valueString(modkey("burnInProbability"));
    if (!burninprob.empty()) {
        mBurnInProbability.setExpression(burninprob);
        lg->debug("AutoManagement burnInProbability is active (value: '{}').", mBurnInProbability.expression());
    }

    mMinHeight = settings.valueDouble(modkey("minHeight"), 20.);

    // output: set up large enough space for all states
    mStateHistogram.resize(Model::instance()->states()->stateIdLookupLength());

    // setup of the management output grid
    auto &grid = Model::instance()->landscape()->grid();
    mGrid.setup(grid.metricRect(), grid.cellsize());
    mGrid.initialize(0);


    lg->debug("AutoManagementModule: params: burnInProbability: '{}', minHeight: '{}'", burninprob, mMinHeight);
    lg->info("Setup of AutoManagementModule '{}' complete.", name());
    lg = spdlog::get("modules");

}

std::vector<std::pair<std::string, std::string> > AutoManagementModule::moduleVariableNames() const
{

    return { {"height", "stand topheight (m)"},
        {"heightIncrement", "lower bound of height increment \n (based on current state &  history) in m/yr"},
        {"lastYear", "simulation year of last management on cell \n 0 if never managed."}
    };

}

double AutoManagementModule::moduleVariable(const Cell *cell, size_t variableIndex) const
{

    switch (variableIndex) {
    case 0: // heihgt
        return cell->state()->topHeight();
    case 1: // height increment
        return cell->heightIncrement();
    case 2: // last year of management
        return mGrid[cell->cellIndex()];
    }
    return 0.;
}

void AutoManagementModule::run()
{
    double p_burnin = 1.;
    if (!mBurnInProbability.isEmpty())
        p_burnin = mBurnInProbability.calculate(Model::instance()->year());

    // reset stats
    std::fill(mStateHistogram.begin(), mStateHistogram.end(), 0);

    lg->debug("Start AutoManagement. BurnInProb: '{}'", p_burnin);

    RectF rect; // rectangle of the cell to process
    std::pair<int, int> stats = {0,0};
    int capcells_tested = 0;


    if (mManagementCapGrid.isEmpty()) {
        // no regional caps, run over the whole grid
        rect = Model::instance()->landscape()->grid().metricRect();
        stats = runArea(rect, p_burnin, -1);

    } else {
        for (int i=0;i < mManagementCapGrid.count(); ++i) {
            double mgmt_cap = mManagementCapGrid[i];
            if (mManagementCapGrid.isNull(mgmt_cap) ||
                !Model::instance()->landscape()->grid().coordValid( mManagementCapGrid.cellCenterPoint(i) ))
                continue;

            ++capcells_tested;
            mgmt_cap *= mManagementCapModifier;
            rect = mManagementCapGrid.cellRect(mManagementCapGrid.indexOf(i));

            auto res = runArea(rect, p_burnin, mgmt_cap);
            stats.first += res.first; // tested
            stats.second += res.second; // managed
            if (lg->should_log(spdlog::level::trace)) {
                lg->trace("Management on cell {}. #tested: '{}', #managed: '{}'", i, stats.first, stats.second);
            }
        }
    }
    lg->info("AutoManagement completed. #tested: '{}', #managed: '{}'. #large scale cells: {}", stats.first, stats.second, capcells_tested);

    // fire output
    Model::instance()->outputManager()->run("AutoManagement");

}

std::pair<int, int> AutoManagementModule::runArea(const RectF &rect, double p_burnin, double mgmt_cap)
{
    const auto &grid = Model::instance()->landscape()->grid();
    RectF arect = rect.cropped(grid.metricRect());
    if (arect != rect) {
        lg->trace("AutoManagementModule: cropping of regional cap cell to the project area.");
        // scale cap to cropped cell area
        mgmt_cap = mgmt_cap * (arect.width()*arect.height()) / (rect.width()*rect.height());
    }

    GridRunner<GridCell> runner(grid, arect);
    bool do_cap = mgmt_cap >= 0.;
    // determine random starting position
    size_t n_cells = (arect.width() * arect.height() ) / (grid.cellsize() * grid.cellsize());
    size_t n_skip = irandom(0, n_cells - 1); // a random spot to start:

    // fast forward to that position
    for (size_t i=0;i<n_skip;++i)
        runner.next();
    auto endcell = runner.current();

    int n_tested = 0;
    int n_managed = 0;
    std::pair<int, int> result;
    // run over all cells on a part of the landscape and check height increment
    while (runner.next() != endcell) {
        if (runner.current() == nullptr) {
            runner.reset(); runner.next();
        }
        if (runner.current()->isNull())
            continue;

        Cell &c = runner.current()->cell();

        if (c.state()->topHeight() >= mMinHeight) {
            ++n_tested;
            double h_inc_thresh = c.state()->value(miIncrementThreshold);
            if (c.heightIncrement() < h_inc_thresh) {
                // height increment is below threshold
                // the cell is managed with a user-defined probability
                double p_manage = c.state()->value(miManagement) * p_burnin;

                if (p_manage==1. || drandom() < p_manage) {
                    // ok, now manage the stand!
                    //if (c.state()->topHeight() < 36)
                    //    --n_tested; // fake
                    // (1) save stats:
                    ++mStateHistogram[ static_cast<size_t>(c.stateId())];
                    // (2) effect of management: a transition to another state
                    state_t new_state = mMgmtMatrix.transition(c.stateId());
                    c.setNewState(new_state);
                    // (3) track changes in grid for output / visualization
                    mGrid[c.cellIndex()] = Model::instance()->year();
                    ++n_managed;
                }
                if (do_cap) {
                    if (n_managed > mgmt_cap) {
                        // stop management
                        lg->debug("Reached management cap of '{}' cells after testing {}% of cells.", mgmt_cap, n_tested/double(n_cells)*100.);
                        break;
                    }
                }
            }
        }

    }
    result.first = n_tested;
    result.second = n_managed;
    return result;

}

