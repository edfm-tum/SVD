#include "automanagementmodule.h"

#include "model.h"
#include "environmentcell.h"
#include "tools.h"
#include "filereader.h"

AutoManagementModule::AutoManagementModule(std::string module_name): Module(module_name, State::None)
{

}

void AutoManagementModule::setup()
{
    lg = spdlog::get("setup");
    lg->info("Setup of AutoManagementModule '{}'", name());
    auto settings = Model::instance()->settings();
    settings.requiredKeys("modules." + name(), {"stateFile", "burnInProbability", "transitionFile" });

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

    // equation for burn-in phase of management
    std::string burninprob = settings.valueString(modkey("burnInProbability"));
    if (!burninprob.empty()) {
        mBurnInProbability.setExpression(burninprob);
        lg->debug("AutoManagement burnInProbability is active (value: '{}').", mBurnInProbability.expression());
    }

    mMinHeight = settings.valueDouble(modkey("minHeight"), 20.);

    // output: set up large enough space for all states
    mStateHistogram.resize(Model::instance()->states()->stateIdLookupLength());
    lg->debug("AutoManagementModule: params: burnInProbability: '{}', minHeight: '{}'", burninprob, mMinHeight);
    lg->info("Setup of AutoManagementModule '{}' complete.", name());
    lg = spdlog::get("modules");

}

std::vector<std::pair<std::string, std::string> > AutoManagementModule::moduleVariableNames() const
{

    return { {"height", "stand topheight (m)"},
        {"heightIncrement", "lower bound of height increment \n (based on current state &  history) in m/yr"}};

}

double AutoManagementModule::moduleVariable(const Cell *cell, size_t variableIndex) const
{

    switch (variableIndex) {
    case 0: // heihgt
        return cell->state()->topHeight();
    case 1: // height increment
        return cell->heightIncrement();
    }
    return 0.;
}

void AutoManagementModule::run()
{
    int n_managed = 0, n_tested = 0;
    double p_burnin = 1.;
    if (!mBurnInProbability.isEmpty())
        p_burnin = mBurnInProbability.calculate(Model::instance()->year());

    // reset stats
    std::fill(mStateHistogram.begin(), mStateHistogram.end(), 0);

    lg->debug("Start AutoManagement. BurnInProb: '{}'", p_burnin);


    // run over all cells on the landscape and check height increment
    for (Cell &c : Model::instance()->landscape()->cells()) {
        if (c.state()->topHeight() >= mMinHeight) {
            ++n_tested;
            double h_inc_thresh = c.state()->value(miIncrementThreshold);
            if (c.heightIncrement() < h_inc_thresh) {
                // height increment is below threshold
                // the cell is managed with a user-defined probability
                double p_manage = c.state()->value(miManagement) * p_burnin;
                if (p_manage==1. || drandom() < p_manage) {
                    // ok, now manage the stand!
                    // (1) save stats:
                    ++mStateHistogram[ static_cast<size_t>(c.stateId())];
                    // (2) effect of management: a transition to another state
                    state_t new_state = mMgmtMatrix.transition(c.stateId());
                    c.setNewState(new_state);
                    ++n_managed;
                }
            }
        }
    }

    lg->info("AutoManagement completed. #tested: '{}', #managed: '{}'", n_tested, n_managed);

    // fire output
    Model::instance()->outputManager()->run("AutoManagement");

}

