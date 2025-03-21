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
#include "fetchdata.h"

#include <regex>

#include "model.h"
#include "tensorhelper.h"
#include "batchdnn.h"
#include "settings.h"
#include "expressionwrapper.h"

#include "modules/simplemanagement/simplemanagementmodule.h"

void FetchData::setup(const Settings * /*settings*/, const std::string & /*key*/, const InputTensorItem & /*item*/)
{

}

void FetchData::fetch(Cell * /* cell */, BatchDNN* /*batch */, size_t /* slot */)
{
}

FetchData *FetchData::createFetchObject(InputTensorItem *def)
{
    FetchData *f=nullptr;
    switch (def->content) {
    case InputTensorItem::Climate:
    case InputTensorItem::State:
    case InputTensorItem::ResidenceTime:
    case InputTensorItem::ResTimeHistory:
    case InputTensorItem::StateHistory:
    case InputTensorItem::SiteNPKA:
    case InputTensorItem::Scalar:
    case InputTensorItem::DistanceOutside:
    case InputTensorItem::Neighbors:
        f = new FetchDataStandard(def);
        break;
    case InputTensorItem::Variable:
        f = new FetchDataVars(def);
        break;
    case InputTensorItem::Function:
        f = new FetchDataFunction(def);
        break;
    default:
        throw std::logic_error("Unknown type of data extractor for: " + def->name);
    }

    return f;
}

void FetchDataStandard::setup(const Settings * /*settings*/, const std::string & /*key*/, const InputTensorItem & item)
{
    if (item.content==InputTensorItem::Climate) {
        // check dimensions
    }
    switch (item.content) {

    // legacy input for Nationalpark Kalkalpen
    case InputTensorItem::SiteNPKA:
        // required columns: availableNitrogen, soilDepth
        i_nitrogen = indexOf(EnvironmentCell::variables(), "availableNitrogen");
        i_soildepth = indexOf(EnvironmentCell::variables(), "soilDepth");
        if (i_nitrogen==-1 || i_soildepth==-1) {
            spdlog::get("dnn")->error("The required columns 'availableNitrogen' and 'soilDepth' are not available in the environment (item: '{}', available: '{}').", item.name, join(EnvironmentCell::variables()));
            throw std::logic_error("Error in setup of SiteNPKA");
        }
        return;

    // distance from edge of the landsacpe (NPKA)
    case InputTensorItem::DistanceOutside:
            i_distance = indexOf(EnvironmentCell::variables(), "distanceOutside");
            if (i_distance==-1 ) {
                spdlog::get("dnn")->error("The required columns 'distanceOutside' is not available in the environment (item: '{}', available: '{}').", item.name, join(EnvironmentCell::variables()));
                throw std::logic_error("Error in setup of DistanceOutside");
            }
            return;

    // state history
    case InputTensorItem::StateHistory:
        if (item.ndim!=1 || item.sizeX != Cell::historySize()) {
            throw logic_error_fmt("Input StateHistory: invalid size: '{}'. Valid is: {}", item.sizeX, Cell::historySize());
        }
        if (item.type != InputTensorItem::DT_INT16)
            throw logic_error_fmt("Input StateHistory: invalid data type '{}', allowed data type is int16", InputTensorItem::datatypeString(item.type));
        return;

    // history of residence times
    case InputTensorItem::ResTimeHistory:
        if (item.ndim!=1 || item.sizeX != Cell::historySize()) {
            throw logic_error_fmt("Input ResidenceTimeHistory: invalid size: '{}'. Valid value is: {}", item.sizeX, Cell::historySize());
        }
        if (item.type != InputTensorItem::DT_FLOAT)
            throw logic_error_fmt("Input ResTimeHistory: only float allowed as data type! (Provided: '{}'", InputTensorItem::datatypeString(item.type));
        return;


    // current state of a cell
    case InputTensorItem::State:
        if (item.type != InputTensorItem::DT_INT16 &&
                item.type != InputTensorItem::DT_UINT16 &&
                item.type != InputTensorItem::DT_INT32)
            throw logic_error_fmt("Input State: invalid data type '{}', allowed data type is float", InputTensorItem::datatypeString(item.type));
        return;

    // current residence time of a cell
    case InputTensorItem::ResidenceTime:
        if (item.type != InputTensorItem::DT_FLOAT)
            throw logic_error_fmt("Input ResidenceTime: invalid data type '{}', allowed data types are: uint16, int16, int32", InputTensorItem::datatypeString(item.type));
        return;




    default: return;
    }

}

void FetchDataStandard::fetch(Cell *cell, BatchDNN *batch, size_t slot)
{
    switch (mItem->content) {
    case InputTensorItem::Climate:
        fetchClimate(cell, batch, slot);
        break;
    case InputTensorItem::State:
        fetchState(cell, batch, slot);
        break;
    case InputTensorItem::StateHistory:
        fetchStateHistory(cell, batch, slot);
        break;

    case InputTensorItem::ResidenceTime:
        fetchResidenceTime(cell, batch, slot);
        break;
    case InputTensorItem::ResTimeHistory:
        fetchResTimeHistory(cell, batch, slot);
        break;
    case InputTensorItem::SiteNPKA:
        fetchSite(cell, batch, slot);
        break;
    case InputTensorItem::Neighbors:
        fetchNeighbors(cell, batch, slot);
        break;
    case InputTensorItem::Scalar:
        // a scalar is already set to the correct value.
        break;
    case InputTensorItem::DistanceOutside:
        fetchDistanceOutside(cell, batch, slot);
        break;

    default:
        throw std::logic_error("InferenceData::fetchData: invalid content type.");

    }

}



void FetchDataStandard::fetchClimate(Cell *cell, BatchDNN* batch, size_t slot)
{

    // the climate data
    const auto &ec = cell->environment();
    auto climate_series = Model::instance()->climate()->series(Model::instance()->year(),
                                                               mItem->sizeX,
                                                               ec->climateId());
    auto n_values = Model::instance()->climate()->nDNNcolumns();

    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap3d<float> *tw = static_cast<TensorWrap3d<float>*>(t);


    if (climate_series.size() == 0 || climate_series.size() != mItem->sizeX || n_values != mItem->sizeY)
        throw logic_error_fmt("FetchDataStandard::fetchClimate: mismatch in dimensions: expected '{}', got '{}' years (rows); expected '{}' columns, got '{}' columns (per year)!",
                              mItem->sizeX, climate_series.size(),
                              mItem->sizeY, n_values);

    if (climate_series[0]->size() < n_values)
        throw logic_error_fmt("FetchDataStandard::fetchClimate: inconsistent climate variables. Number of values for DNN: {}, total number of values {}.", n_values, climate_series[0]->size());

    // copy the climate data to the tensors
    // Note that data transformations are applied already during loading of climate data
    size_t i = 0;
    for (const std::vector<float> *p : climate_series) {
        float *d = tw->row(slot, i++);
        memcpy(d, p->data(), sizeof(float) * n_values);
    }

}

void FetchDataStandard::fetchState(Cell *cell, BatchDNN* batch, size_t slot)
{
    // the current state
    TensorWrapper *t = batch->tensor(mItem->index);
    if (t->dataType() == InputTensorItem::DT_UINT16) {
        TensorWrap2d<short int> *tw = static_cast<TensorWrap2d<short int>*>(t);
        short int *p = tw->example(slot);
        // stateId starts with 1, the state tensor is 0-based
        *p = cell->stateId(); // TODO: check!
        return;
    }
    if (t->dataType() == InputTensorItem::DT_INT16) {
        TensorWrap2d<short int> *tw = static_cast<TensorWrap2d<short int>*>(t);
        short int *p = tw->example(slot);
        // stateId starts with 1, the state tensor is 0-based
        *p = cell->stateId();
        return;
    }

    if (t->dataType() == InputTensorItem::DT_INT32) {
        TensorWrap2d<int32_t> *tw = static_cast<TensorWrap2d<int32_t>*>(t);
        int32_t *p = tw->example(slot);
        // stateId starts with 1, the state tensor is 0-based
        *p = cell->stateId();
        return;
    }
    throw logic_error_fmt("FetchDataStandard:fetchState: invalid data type (allowed: uint16, int16, int32){}", "");

}

void FetchDataStandard::fetchResidenceTime(Cell *cell, BatchDNN* batch, size_t slot)
{
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<float> *tw = static_cast<TensorWrap2d<float>*>(t);
    float *p = tw->example(slot);
    // TODO: residence time, now fixed divide by 10
    *p = static_cast<float>(cell->residenceTime() / 10.f);
}

void FetchDataStandard::fetchStateHistory(Cell *cell, BatchDNN *batch, size_t slot)
{
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<state_t> *tw = static_cast<TensorWrap2d<state_t>*>(t);
    const state_t *state_history = cell->stateHistory();
    state_t *p = tw->example(slot);
    for (size_t i = 0; i<cell->historySize(); ++i) {
        *p++ = *state_history++;
    }
}

void FetchDataStandard::fetchResTimeHistory(Cell *cell, BatchDNN *batch, size_t slot)
{
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<float> *tw = static_cast<TensorWrap2d<float>*>(t);
    const restime_t *state_history = cell->resTimeHistory();
    float *p = tw->example(slot);
    for (size_t i = 0; i<cell->historySize(); ++i) {
        *p++ = *state_history / 10.f; // divide by 10
        state_history++;
    }
}


void FetchDataStandard::fetchNeighbors(Cell *cell, BatchDNN* batch, size_t slot)
{
    const size_t n_neighbors = 62; // 2x32
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<float> *tw = static_cast<TensorWrap2d<float>*>(t);
    float *p = tw->example(slot);

    auto neighbors = cell->neighborSpecies();
    if (neighbors.size() != n_neighbors)
        throw std::logic_error("Invalid number of neighbors...");

    for (size_t i=0;i<n_neighbors;++i)
        *p++ = static_cast<float>(neighbors[i]);

}

void FetchDataStandard::fetchSite(Cell *cell, BatchDNN* batch, size_t slot)
{
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<float> *tw = static_cast<TensorWrap2d<float>*>(t);
    float *p = tw->example(slot);
    // site: nitrogen/soil-depth
    const auto &ec = cell->environment();
    // TODO: transformation...
    *p++ = static_cast<float>( (ec->value(static_cast<size_t>(i_nitrogen)) -58.500)/41.536 );
    *p++ = static_cast<float>( (ec->value(static_cast<size_t>(i_soildepth))-58.500)/41.536 );
}

void FetchDataStandard::fetchDistanceOutside(Cell *cell, BatchDNN* batch, size_t slot)
{
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<float> *tw = static_cast<TensorWrap2d<float>*>(t);
    float *p = tw->example(slot);
    const auto &ec = cell->environment();

    *p = static_cast<float>( ec->value(static_cast<size_t>(i_distance)) );

}

void FetchDataVars::setup(const Settings *settings, const std::string &key, const InputTensorItem &item)
{
    // expects as many expressions as 'sizeX', one-dimensional, and datatype float
    // the expressions are given as a string with the "transformation" key, and each
    // expression is within curly braces: {expr1}, {expr2}, {expr3}, ... {exprN}
    auto lg = spdlog::get("setup");
    std::string tlist = settings->valueString(key + ".transformations");
    lg->debug("Set up expressions: {}", tlist);
    bool has_error = false;
    try {
      std::regex re("\\{([^\\}]*)\\}");
      std::sregex_iterator next(tlist.begin(), tlist.end(), re);
      std::sregex_iterator end;
      while (next != end) {
        std::smatch match = *next;
        lg->debug("expression: {}", match.str(1));

        mExpressions.push_back(new Expression(match.str(1)));
        next++;
      }
      if (item.type != InputTensorItem::DT_FLOAT) {
          lg->error("Setup of Tensor {} (type: {}): Datatype 'float' expected.", item.name, item.contentString(item.content));
          has_error = true;
      }
      if (item.ndim != 1 || item.sizeX != mExpressions.size()) {
          lg->error("Setup of Tensor {} (type: {}): Number of dimensions wrong (expected: 1), or number of expressions ({}) does not match the size of the tensor ({}).", item.name,
                    item.contentString(item.content),
                    mExpressions.size(), item.sizeX);
          has_error = true;
      }
      if (has_error)
          throw std::logic_error("Error in setting up an Input tensor. Check the log.");

    } catch (std::regex_error& e) {
      throw std::logic_error(to_string("Error in setting up expression (regexp error): ") + e.what());
    }
}

void FetchDataVars::fetch(Cell *cell, BatchDNN *batch, size_t slot)
{
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<float> *tw = static_cast<TensorWrap2d<float>*>(t);
    float *p = tw->example(slot);
    CellWrapper cw(cell);
    for (auto &expr : mExpressions) {
        *p++ = static_cast<float>( expr->calculate(cw) );
    }

}


static std::vector<std::string> FetchFunctionList = {"Invalid", "DistToSeedSource", "SimpleManagement"};
void FetchDataFunction::setup(const Settings *settings, const std::string &key, const InputTensorItem &item)
{
    auto lg = spdlog::get("setup");
    std::string fn = settings->valueString(key + ".function");
    lg->debug("Set up input for function: {}", fn);
    int idx = indexOf(FetchFunctionList, fn);
    if (idx < 1) {
        throw logic_error_fmt("Setup of FetchDataFunction for input '{}': provided 'function' ('{}') is not valid. Available are: {}",item.name, fn, join(FetchFunctionList, ","));
    }

    mFn = static_cast<EFunctions>(idx);
    switch (mFn) {
    case DistToSeedSource:
        setupDisttoSeedSource();
        break;
    case SimpleManagement:
        setupSimpleManagement();
        break;
    default: throw logic_error_fmt("setup of FetchDataFunction: invalid function for {}", item.name);
    }

}

void FetchDataFunction::fetch(Cell *cell , BatchDNN *batch, size_t slot)
{
    TensorWrapper *t = batch->tensor(mItem->index);
    TensorWrap2d<float> *tw = static_cast<TensorWrap2d<float>*>(t);
    float *p = tw->example(slot);

    float value;
    switch (mFn) {
    case DistToSeedSource:
        value = calculateDistToSeedSource(cell);
        break;
    case SimpleManagement: {
        float rActivity, rTime;
        calculateSimpleManagement(cell, rActivity, rTime);
        *p++ = rActivity;
        *p = rTime;
        return;
    }
    default:
        throw logic_error_fmt("FetchDataFunction: invalid Function: {}.", mFn );
    }
    *p = value;

}

void FetchDataFunction::setupDisttoSeedSource()
{
    // check if required columns are available:
    if (State::valueIndex("seedSourceType") < 0 || State::valueIndex("seedTargetType") < 0)
        throw std::logic_error("The 'DistToSeedSource' function requires the state properties 'seedSourceType' and 'seedTargetType'.");
    mD2S_seed_source = static_cast<size_t>(State::valueIndex("seedSourceType"));
    mD2S_target = static_cast<size_t>(State::valueIndex("seedTargetType"));
}


// distances from center point (X)
// 4 4 3 4 4
// 4 2 1 2 4
// 3 1 X 1 3
// 4 2 1 2 4
// 4 4 3 4 4
// 1..4: distance classes (50m, 100m, 150m, 200m)
static std::vector<std::pair<Point, float> > dist2seeds = {
    {{1,0},50.f }, {{0,1},50.f }, {{-1,0},50.f }, {{0,-1},50.f },  // distances 1
    {{-1,-1},100.f }, {{1,-1},100.f }, {{-1,1},100.f }, {{1,1},100.f }, // distances 2
    {{2,0},150.f }, {{0,2},150.f }, {{-2,0},150.f }, {{0, -2},150.f }, // distances 3
    {{-2,-2},200.f }, {{-1,-2},200.f }, {{1,-2},200.f }, {{2,-2},200.f }, {{-2,-1},200.f }, {{2,-1},200.f }, // distances 4 (upper half)
    {{-2, 2},200.f }, {{-1, 2},200.f }, {{1, 2},200.f }, {{2, 2},200.f }, {{-2, 1},200.f }, {{2, 1},200.f } // distances 4 (lower half)
};

float FetchDataFunction::calculateDistToSeedSource(Cell *cell)
{
    if (cell->state()==nullptr)
        return 0.f;

    auto &grid =  Model::instance()->landscape()->grid();

    size_t target = static_cast<size_t>(cell->state()->value(mD2S_target));

    Point center = grid.indexOf(cell->cellIndex());

    for (const auto &p : dist2seeds) {
        if (grid.isIndexValid(center + p.first)) {
            GridCell &c = grid.valueAtIndex(center + p.first);
            if (!c.isNull() && c.cell().state()!=nullptr)
                if (static_cast<size_t>(c.cell().state()->value(mD2S_seed_source)) == target) {
                    // found a distance
                    return p.second / 1000.f;
                }
        }
    }

    // brute force
    float min_dist_sq = 100000.f;
    for (int y=-12; y<=12; ++y)
        for (int x=-12; x<=12; ++x)
            if (grid.isIndexValid(center + Point(x,y))) {
                GridCell &c = grid.valueAtIndex(center + Point(x,y));
                if (!c.isNull() && c.cell().state()!=nullptr)
                    if (static_cast<size_t>(c.cell().state()->value(mD2S_seed_source)) == target) {
                        // found a seed pixel: (x-0.5)*(x-0.5)=x^2-x+0.25
                        min_dist_sq = std::min(min_dist_sq, x*x-x+0.25f + y*y-y+0.25f);
                    }
            }
    float min_dist = std::min(sqrt(min_dist_sq), 12.5f); // training data goes to 1250m distance, unit here is 100m steps
    return min_dist / 10.f; // convert to m/1000

}

void FetchDataFunction::setupSimpleManagement()
{
    // check if required modules / variables are available
    mMgmtModule = dynamic_cast<SimpleManagementModule*>(Model::instance()->moduleByName("mgmt")) ;
    if (!mMgmtModule)
        throw logic_error_fmt("Setup of management: the required module 'mgmt' is not available!");
}

void FetchDataFunction::calculateSimpleManagement(Cell *cell, float &rActivity, float &rTime)
{
    mMgmtModule->managementActivity(cell, rActivity, rTime);
}
