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
#include "transitionmatrix.h"

#include "spdlog/spdlog.h"

#include "model.h"
#include "states.h"

#include "strtools.h"
#include "filereader.h"
#include "randomgen.h"

#include "expressionwrapper.h"
#include "expression.h"

TransitionMatrix::TransitionMatrix()
{

}

bool TransitionMatrix::load(const std::string &filename)
{
    FileReader rdr(filename);
    rdr.requiredColumns({"stateId", "key", "targetId", "p"});
    auto is = rdr.columnIndex("stateId");
    auto ik = rdr.columnIndex("key");
    auto it = rdr.columnIndex("targetId");
    auto ip = rdr.columnIndex("p");
    auto ipmin = rdr.columnIndex("pmin");
    auto ipmax = rdr.columnIndex("pmax");
    auto iexpr = rdr.columnIndex("expression");
    bool has_pminmax = ipmin != std::string::npos && ipmax != std::string::npos;
    if (has_pminmax)
        spdlog::get("setup")->debug("Transition matrix includes pmin / pmax columns.");
    int n=0;
    while (rdr.next()) {
        // read line
        state_t id = state_t( rdr.value(is) );
        int key = int( rdr.value(ik) );
        state_t target = state_t( rdr.value(it) );
        double p =  rdr.value(ip);
        if (p<0. || p>1.)
            throw logic_error_fmt("TransitionMatrix: invalid probability {}. Allowed is the range 0..1", p);

        auto &e = mTM[ {id, key} ];
        e.push_back(STransitionItem(target, p));
        if (has_pminmax) {
            double pmin = rdr.value(ipmin);
            double pmax = rdr.value(ipmax);
            if (pmin<0. || pmin>1. || pmax<0. || pmax>1.)
                throw logic_error_fmt("TransitionMatrix: invalid probability for pmin/pmax ({}/{}). Allowed is the range 0..1", pmin, pmax);
            if (pmax > 0.) {
                e.back().pmin = pmin;
                e.back().pmax = pmax;
            }
        }
        if (iexpr != std::numeric_limits<std::size_t>::max() )
            if (!rdr.valueString(iexpr).empty()) {
                //CellWrapper wrap(nullptr);
                e.back().expr = std::unique_ptr<Expression>(new Expression(rdr.valueString(iexpr)));
                //e.back().expr->parse(&wrap); // parse immediately
            }
        ++n;
    }

    // TODO: check transition matrix: states have to be valid, p should sum up to 1
    if (spdlog::get("setup")->should_log(spdlog::level::trace)) {
        std::stringstream ss;
        for (const auto &sk : mTM) {
            state_t st = static_cast<state_t>(sk.first.first);
            const auto &s = Model::instance()->states()->stateById(st);

            ss << fmt::format("State: #{}: '{}' (key: '{}')", st, s.name(), sk.first.second) << std::endl;
            for (const auto &item : sk.second) {
                const auto &s_target = Model::instance()->states()->stateById(item.state);
                ss << fmt::format("|- #{}: '{}': p={}", item.state, s_target.name(), item.prob);
                if (item.has_minmax())
                    ss << fmt::format(" pmin/pmax: {}/{}", item.pmin, item.pmax);
                if (item.expr)
                    ss << fmt::format(" expr: '{}'", item.expr->expression());
                ss << std::endl;
            }
            ss << "====================" << std::endl;
        }
        spdlog::get("setup")->trace("Transition matrix dump: \n{}", ss.str());
    }


    spdlog::get("setup")->debug("Loaded transition matrix for {} states from file '{}' (processed {} records).", mTM.size(), filename, n);
    return true;
}

state_t TransitionMatrix::transition(state_t stateId, int key, CellWrapper *cell)
{
    auto it = mTM.find({stateId, key});
    if (it == mTM.end()) {
        throw logic_error_fmt("TransitionMatrix: no valid transitions found for state {}, key {}", stateId, key);
    }

    const auto &prob = it->second;
    if (prob.size() == 1 && prob.front().prob == 1.)
        return prob.front().state;
    // choose a state probabilistically
    bool has_expr = false;
    bool has_self = false;
    double p_sum = 0.;
    for (const auto &item : prob) {
        p_sum+=item.prob;

        if (item.state == stateId)
            has_self = true;

        if (item.expr) {
            has_expr=true;
            if (!cell)
                logic_error_fmt("TransitionMatrix: a transition with an expression: {}, state {} key {} is used, but there is no valid cell.", item.expr->expression(), stateId, key);

        }
    }

    // special case:
    if (has_expr) {
        // we need to run the expressions and store their result
        std::vector<double> ps(prob.size());
        double *cp = &ps.front();
        p_sum = 0.;
        for (const auto &item : prob) {
            *cp = item.prob;
            if (item.expr) {
                auto pexpr = std::max( item.expr->calculate(*cell), 0.); // multiply base probability with result of the expression, do not allow negative probability
                if (!item.has_minmax())
                    *cp *= pexpr;
                else
                    *cp = item.pmin + (item.pmax-item.pmin) * std::min(pexpr, 1.); // value between pmin and pmax
            }
            p_sum+= *cp++;
        }
        if (p_sum>1. && !has_self)
            throw logic_error_fmt("TransitionMatrix: the sum of probababilities for states (excl.self) are > 1. ");

        // see almost identical code below
        double p;
        if (has_self) {
            p = nrandom(0, p_sum);
        } else {
            p = drandom();
            if (p>p_sum)
                return stateId; // no change
        }
        p_sum = 0.;
        for (size_t i=0;i<ps.size();++i) {
            p_sum += ps[i];
            if (p < p_sum)
                return prob[i].state;
        }
        if (!has_self)
            return stateId;
        throw logic_error_fmt("TransitionMatrix: no valid target found for state {}, key {}. Maybe all probabilites = 0?", stateId, key);

    }
    if (p_sum>1. && !has_self)
        throw logic_error_fmt("TransitionMatrix: the sum of probababilities for states (excl.self) are > 1. ");
    double p;

    /*  Two options:
     *  (a) (a prob for remaining the same state is provided): probs are scaled (incl. expressions)
     *  (b) (no prob for remaining): no scaling, all other paths can be scaled, the rest (up to 1) is "remain"
     * */
    if (has_self) {
        p = nrandom(0, p_sum);
    } else {
        p = drandom(); // 0..1
        if (p > p_sum)
            return stateId; // no change (shortcut)
    }

    p_sum = 0.;

    for (const auto &item : prob) {
        p_sum+=item.prob;
        if (p < p_sum)
            return item.state;
    }
    if (!has_self)
        return stateId; // same state

    throw logic_error_fmt("TransitionMatrix: no valid target found for state {}, key {}", stateId, key);
}
