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

#include "expression.h"
#include "strtools.h"

#include "expressionwrapper.h"

#include <algorithm>
#include <cassert>
#include <mutex>
#include <cmath>
#include "randomgen.h"

/** @class Expression
  An expression engine for mathematical expressions provided as strings.
  @ingroup tools
  @ingroup script
  The main purpose is fast execution speed.
  notes regarding the syntax:
  +,-,*,/ as expected, additionally "^" for power.
  mod(x,y): modulo division, gets remainder of x/y
  functions:
    - sin cos tan
    - exp ln sqrt
    - min max: variable number of arguments, e.g: min(x,y,z)
    - if: if(condition, true, false): if condition=true, return true-case, else false-case. note: both (true, false) are evaluated anyway!
    - incsum: ?? incremental sum - currently not supported.
    - polygon: special function for polygons. polygon(value, x1,y1, x2,y2, x3,y3, ..., xn,yn): return is: y1 if value<x1, yn if value>xn, or the lineraly interpolated numeric y-value.
    - sigmoid: returns a sigmoid function. sigmoid(value, type, param1, param2). see udfSigmoid() for details.
    - rnd rndg: random functions; rnd(from, to): uniform random number, rndg(mean, stddev): gaussian randomnumber (mean and stddev in percent!)
    - limit: combined min/max, lets you limt a number in a provided range
    The Expression class also supports some logical operations:
    (logical) True equals to "1", "False" to zero. The precedence rules for parentheses...
    - and
    - or
    - not
  @par Using Model Variables
  With the help of descendants of ExpressionWrapper values of model objects can be accessed. Example Usage:
  @code
  TreeWrapper wrapper;
  Expression basalArea("dbh*dbh*3.1415/4", &wrapper); // expression for basal area, add wrapper (see also setModelObject())
  AllTreeIterator at(GlobalSettings::instance()->model()); // iterator to iterate over all tree in the model
  double sum;
  while (Tree *tree = at.next()) {
      wrapper.setTree(tree); // set actual tree
      sum += basalArea.execute(); // execute calculation
  }
  @endcode

  Be careful with multithreading:
  Now the calculate(double v1, double v2) as well as the calculate(wrapper, v1,v2) are thread safe. execute() accesses the internal variable list and is therefore not thredsafe.
  A threadsafe version exists (executeLocked()). Special attention is needed when using setVar() or addVar().

*/


#define opEqual 1
#define opGreaterThen 2
#define opLowerThen 3
#define opNotEqual 4
#define opLowerOrEqual 5
#define opGreaterOrEqual 6
#define opAnd 7
#define opOr  8

static std::vector<std::string> mathFuncList={"sin", "cos", "tan",
                                       "exp", "ln", "sqrt",
                                       "min", "max", "if",
                                       "incsum", "polygon", "mod", "sigmoid", "rnd", "rndg", "limit",  "round", "in",
                                             "localNB", "intermediateNB", "globalNB", "distance", "speciesProportion"};
const int  MaxArgCount[23]={1,1,1,1,  1, 1,   -1, -1, 3, 1, -1, 2, 4, 2, 2, 3, 1, -1,    -1,-1,-1,-1,-1};
#define    AGGFUNCCOUNT 6
static std::string AggFuncList[AGGFUNCCOUNT]={"sum", "avg", "max", "min", "stddev", "variance"};

// constants (static)
std::vector<std::string> Expression::mConstants;

void Expression::setConstants(const std::vector<std::string> &consts)
{
    mConstants.clear();
    for (auto s : consts)
        mConstants.push_back(s);

}

bool Expression::mLinearizationAllowed = false;
Expression::Expression()
{
    mModelObject = nullptr;
    m_externVarSpace=nullptr;
    m_execList=nullptr;
}


Expression::ETokType  Expression::next_token()
{
    m_tokCount++;
    m_lastState=m_state;
    // eliminate whitespaces
    while (strchr(" \t\n\r", *m_pos) && *m_pos)
        m_pos++;

    if (*m_pos==0) {
        m_state=etStop;
        m_token="";
        return etStop;
    }
    // eliminate whitespaces
    while (strchr(" \t\n\r", *m_pos))
        m_pos++;
    if (*m_pos==',')
    {

        m_token=*m_pos++;
        m_state=etDelimeter;
        return etDelimeter;
    }
    if (strchr("+-*/(){}^", *m_pos)) {
        m_token=*m_pos++;
        m_state=etOperator;
        return etOperator;
    }
    if (strchr("=<>", *m_pos)) {
        m_token=*m_pos++;
        if (*m_pos=='>' || *m_pos=='=')
            m_token+=*m_pos++;
        m_state=etCompare;
        return etCompare;
    }
    if (*m_pos>='0' && *m_pos<='9') {
        // number
        m_token = to_string(atof(m_pos));

        while (strchr("0123456789.",*m_pos) && *m_pos!=0)
            m_pos++;  // allowed values

        m_state=etNumber;
        return etNumber;
    }

    if ((*m_pos>='a' && *m_pos<='z') || (*m_pos>='A' && *m_pos<='Z') || (*m_pos=='_')) {
        // function ... find brace
        m_token="";
        while (( (*m_pos>='a' && *m_pos<='z') || (*m_pos>='A' && *m_pos<='Z')
                || (*m_pos>='0' && *m_pos<='9') || (*m_pos=='_' || *m_pos=='.') )
            && *m_pos!='(' && m_pos!=nullptr )
            m_token+=*m_pos++;
        // brace -> function, else variable.
        if (*m_pos=='(' || *m_pos=='{') {
            m_pos++; // skip brace
            m_state=etFunction;
            return etFunction;
        } else {
            if (lowercase(m_token)=="and" || lowercase(m_token)=="or") {
                m_state=etLogical;
                return etLogical;
            } else {
                m_state=etVariable;
                // support for pseudo-literals 'true' and 'false'
                if (m_token=="true") { m_state=etNumber; m_token="1"; return etNumber; }
                if (m_token=="false") { m_state=etNumber; m_token="0"; return etNumber; }
                // and constants
                if (contains(mConstants, m_token)) {
                    m_state = etNumber;
                    m_token = to_string( indexOf(mConstants, m_token) );
                    return etNumber;
                }
                return etVariable;
            }
        }
    }
    m_state=etUnknown;
    return etUnknown; // in case no match was found

}

Expression::~Expression()
{
    if (m_execList)
        delete[] m_execList;
}


/** sets expression @p expr and checks the syntax (parse).
    Expressions are setup with strict = false, i.e. no fixed binding of variable names.
  */
void Expression::setAndParse(const std::string &expr)
{
    setExpression(expr);
    m_strict = false; 
    parse();
}

/// set the current expression.
/// do some preprocessing (e.g. handle the different use of ",", ".", ";")
void Expression::setExpression(const std::string& aExpression)
{
    m_expression=unquote(aExpression);

    m_expr=const_cast<char*>(m_expression.c_str());

    m_pos=m_expr;  // set starting point... 

    for (int i=0; i<MAXLOCALVAR; i++)
        m_varSpace[i]=0.;
    m_parsed=false;
    m_catchExceptions = false;
    m_errorMsg = "";

    mModelObject = nullptr;
    m_externVarSpace=nullptr;

    m_strict=true; // default....
    m_incSumEnabled=false;
    m_empty= (m_expression=="") ;
    // Buffer:
    m_execListSize = 5; // inital value...
    m_execList = new ExtExecListItem[m_execListSize]; // init

    mLinearizeMode = 0; // linearization is switched off
    mScriptIndexFunc=nullptr;
    mScriptValueFunc=nullptr;
}


static std::mutex parse_mutex;
void  Expression::parse(ExpressionWrapper *wrapper)
{
    std::lock_guard<std::mutex> guard(parse_mutex);
    if (m_parsed)
        return;
    try {
        ExpressionWrapper *old_wrap=mModelObject;
        if (wrapper) {
           mModelObject = wrapper;
        }
        // Picus compatibility with old functions:
        if (m_expression.find(';')!=std::string::npos) {
            // e.g. change from "polygon(x; 0; 0,3; 0.5)" -> "polygon(x, 0.3, 0.5)"
            //std::replace(m_expression.begin(), m_expression.end(), ',', '.');
            //std::replace(m_expression.begin(), m_expression.end(), ';', ',');
            throw std::logic_error("Expression contains ';':" + m_expression);
       }

        //
        m_tokString="";
        m_state=etUnknown;
        m_lastState=etUnknown;
        m_constExpression=true;
        m_execIndex=0;
        m_tokCount=0;
        int AktTok;
        next_token();
        while (m_state!=etStop) {
            m_tokString+="\n"+m_token;
            AktTok=m_tokCount;
            parse_levelL0();  // start with logical level 0
            if (AktTok==m_tokCount)
                throw std::logic_error("Expression::parse(): Unbalanced Braces. In:" + m_expression);
            if (m_state==etUnknown){
                m_tokString+="\n***Error***";
                throw std::logic_error("Expression::parse(): Syntax error, token: " + m_token);
            }
        }
        m_empty = (m_execIndex == 0);
        m_execList[m_execIndex].Type=etStop;
        m_execList[m_execIndex].Value=0;
        m_execList[m_execIndex++].Index=0;
        checkBuffer(m_execIndex);
        
        compile(); // NEW: compile to bytecode
        
        m_parsed=true;

        mModelObject = old_wrap;

    } catch (const std::exception& e) {
        m_errorMsg ="Expression::parse: Error in: " + m_expression +":" +  e.what();
        throw std::logic_error(m_errorMsg);
    }
}

void  Expression::parse_levelL0()
{
    // logical operations  (and, or, not)
    std::string op;
    parse_levelL1();

    while (m_state==etLogical)  {
        op=lowercase(m_token);
        next_token();
        parse_levelL1();
        int logicaltok=0;
        if (op=="and") logicaltok=opAnd;
        if (op=="or") logicaltok=opOr;


        m_execList[m_execIndex].Type=etLogical;
        m_execList[m_execIndex].Value=0;
        m_execList[m_execIndex++].Index=logicaltok;
        checkBuffer(m_execIndex);
    }
}

void  Expression::parse_levelL1()
{
    // logic operations (<,>,=,...)
    std::string op;
    parse_level0();
    while (m_state==etCompare)  {
        op=m_token;
        next_token();
        parse_level0();
        int logicaltok=0;
        if (op=="<") logicaltok=opLowerThen;
        if (op==">") logicaltok=opGreaterThen;
        if (op=="<>") logicaltok=opNotEqual;
        if (op=="<=") logicaltok=opLowerOrEqual;
        if (op==">=") logicaltok=opGreaterOrEqual;
        if (op=="=")  logicaltok=opEqual;

        m_execList[m_execIndex].Type=etCompare;
        m_execList[m_execIndex].Value=0;
        m_execList[m_execIndex++].Index=logicaltok;
        checkBuffer(m_execIndex);
    }
}

void  Expression::parse_level0()
{
    // plus and minus
    std::string op;
    parse_level1();

    while (m_token=="+" || m_token=="-")  {
        op=m_token;
        next_token();
        parse_level1();
        m_execList[m_execIndex].Type=etOperator;
        m_execList[m_execIndex].Value=0;
        m_execList[m_execIndex++].Index=op.at(0);///op.constData()[0];
        checkBuffer(m_execIndex);
    }

}

void  Expression::parse_level1()
{
    // divide and multiply
    std::string op;
    parse_level2();
    while (m_token=="*" || m_token=="/") {
        op=m_token;
        next_token();
        parse_level2();
        m_execList[m_execIndex].Type=etOperator;
        m_execList[m_execIndex].Value=0;
        m_execList[m_execIndex++].Index=op.at(0);
        checkBuffer(m_execIndex);
    }
}

void  Expression::atom()
{
    if (m_state==etVariable || m_state==etNumber) {
        if (m_state==etNumber) {
            double result=atof(m_token.c_str());
            m_execList[m_execIndex].Type=etNumber;
            m_execList[m_execIndex].Value=result;
            m_execList[m_execIndex++].Index=-1;
            checkBuffer(m_execIndex);
        }
        if (m_state==etVariable) {
            if (!m_strict) // in strict mode, the variable must be available by external bindings. in "lax" mode, the variable is added when encountered first.
                addVar(m_token);
            m_execList[m_execIndex].Type=etVariable;
            m_execList[m_execIndex].Value=0;
            m_execList[m_execIndex++].Index=getVarIndex(m_token);
            checkBuffer(m_execIndex);
            m_constExpression=false;
        }
        next_token();
    } else if (m_state==etStop || m_state==etUnknown)
        throw std::logic_error("Unexpected end of Expression: " + m_expression);
}


void  Expression::parse_level2()
{
    // x^y
    parse_level3();
    while (m_token=="^") {
        next_token();
        parse_level3();
        m_execList[m_execIndex].Type=etOperator;
        m_execList[m_execIndex].Value=0;
        m_execList[m_execIndex++].Index='^';
        checkBuffer(m_execIndex);
    }
}
void  Expression::parse_level3()
{
    // unary operator (- and +)
    std::string op;
    op=m_token;
    bool Unary=false;
    if (op=="-" && (m_lastState==etOperator || m_lastState==etUnknown || m_lastState==etCompare || m_lastState==etLogical || m_lastState==etFunction)) {
        next_token();
        Unary=true;
    }
    parse_level4();
    if (Unary && op=="-") {
        m_execList[m_execIndex].Type=etOperator;
        m_execList[m_execIndex].Value=0;
        m_execList[m_execIndex++].Index='_';
        checkBuffer(m_execIndex);
    }

}

void  Expression::parse_level4()
{
    // braces and functions
    std::string func;
    atom();
    if (m_token=="(" || m_state==etFunction) {
        func=m_token;
        if (func=="(")   // brace
        {
            next_token();
            parse_levelL0();
        }   else {       // function...
            int argcount=0;
            int idx=getFuncIndex(func);
            next_token();
            // multiple args
            while (m_token!=")") {
                argcount++;
                parse_levelL0();
                if (m_state==etDelimeter)
                    next_token();
            }
            if (MaxArgCount[idx]>0 && MaxArgCount[idx]!=argcount)
                throw std::logic_error( "Function " + func + " assumes " + to_string(MaxArgCount[idx]) + " arguments!");
            m_execList[m_execIndex].Type=etFunction;
            m_execList[m_execIndex].Value=argcount;
            m_execList[m_execIndex++].Index=idx;
            checkBuffer(m_execIndex);
        }
        if (m_token!="}" && m_token!=")") // error
            throw std::logic_error("Expression::unbalanced number of parentheses in [" + m_expression + "]");
        next_token();
    }
}

void Expression::setVar(const std::string& Var, double Value)
{
    if (!m_parsed)
        parse();
    int idx=getVarIndex(Var);
    if (idx>=0 && idx<MAXLOCALVAR)
        m_varSpace[idx]=Value;
    else
        throw std::logic_error("Invalid variable " + Var);
}

double Expression::calculate(const double Val1, const double Val2, const bool forceExecution) const
{
    if (mLinearizeMode>0 && !forceExecution) {
        if (mLinearizeMode==1)
            return linearizedValue(Val1);
        return linearizedValue2d(Val1, Val2); // matrix case
    }
    double var_space[MAXLOCALVAR];
    var_space[0]=Val1;
    var_space[1]=Val2;
    m_strict=false;
    return execute(var_space); // execute with local variables on stack
}

double Expression::calculate(ExpressionWrapper &object, const double variable_value1, const double variable_value2) const
{
    double var_space[MAXLOCALVAR];
    var_space[0] = variable_value1;
    var_space[1]=variable_value2;
    // m_strict=false;
    return execute(var_space,&object); // execute with local variables on stack
}


int Expression::getFuncIndex(const std::string& functionName)
{
    int idx=index_of(mathFuncList, functionName);
    if (idx<0)
        throw std::logic_error("Function " + functionName + " not defined!");
    return idx;
}

void Expression::compile() {
    m_program.clear();
    m_program.reserve(static_cast<size_t>(m_execIndex) + 1);

    // Iterating the old list (m_execList is a C-array, m_execIndex is count)
    for (int i = 0; i < m_execIndex; ++i) {
        const auto& item = m_execList[i];
        Instruction instr;
        std::memset(&instr, 0, sizeof(Instruction)); // Clear padding

        switch (item.Type) {
        case etNumber:
            instr.code = OP_PUSH_IMM;
            instr.data.val = item.Value;
            break;

        case etVariable:
            instr.data.index = item.Index;

            if (item.Index < 100) {
                instr.code = OP_LOAD_LOCAL; // on stack
            } else if (item.Index < 1000) {
                instr.code = OP_LOAD_MODEL_VAR; // dynamic variable from the model
            } else {
                instr.code = OP_LOAD_EXTERN_VAR; // variable from an external list of vars
            }
            break;

        case etOperator:
            switch (item.Index) {
            case '+': instr.code = OP_ADD; break;
            case '-': instr.code = OP_SUB; break;
            case '*': instr.code = OP_MUL; break;
            case '/': instr.code = OP_DIV; break;
            case '^': instr.code = OP_POW; break;
            case '_': instr.code = OP_NEG; break;
            }
            break;

        case etFunction:
            instr.data.count = (int)item.Value; // Store argument count
            switch (item.Index) {
            case 0: instr.code = OP_SIN; break;
            case 1: instr.code = OP_COS; break;
            case 2: instr.code = OP_TAN; break;
            case 3: instr.code = OP_EXP; break;
            case 4: instr.code = OP_LOG; break;
            case 5: instr.code = OP_SQRT; break;
            case 6: instr.code = OP_MIN; break;
            case 7: instr.code = OP_MAX; break;
            case 8: instr.code = OP_IF; break;
            // ... custom functions
            case 9: instr.code = OP_INCSUM; break;
            case 10: instr.code = OP_POLYGON; break;
            case 11: instr.code = OP_MODULO; break;
            case 12: instr.code = OP_SIGMOID; break;
            case 13: instr.code = OP_RND; break;
            case 14: instr.code = OP_RNDG; break;
            case 15: instr.code = OP_LIMIT; break;
            case 16: instr.code = OP_ROUND; break;
            case 17: instr.code = OP_IN; break;
            // SVD specific
            case 18: instr.code = OP_LOCALNB; break;
            case 19: instr.code = OP_INTERMEDIATENB; break;
            case 20: instr.code = OP_GLOBALNB; break;
            case 21: instr.code = OP_DISTANCE; break;
            case 22: instr.code = OP_SPECIESPROPORTION; break;
            }
            break;

        case etCompare:
            switch (item.Index) {
            case opEqual: instr.code = OP_EQ; break;
            case opNotEqual: instr.code = OP_NE; break;
            case opLowerThen: instr.code = OP_LT; break;
            case opGreaterThen: instr.code = OP_GT; break;
            case opGreaterOrEqual: instr.code = OP_GE; break;
            case opLowerOrEqual: instr.code = OP_LE; break;
            }
            break;

        case etLogical:
            switch (item.Index) {
            case opAnd: instr.code = OP_AND; break;
            case opOr: instr.code = OP_OR; break;
            }
            break;

        default: break;
        }
        m_program.push_back(instr);
    }

    Instruction stop;
    stop.code = OP_STOP;
    m_program.push_back(stop);
}

double Expression::execute(double *varlist, ExpressionWrapper *object, bool *rLogicResult) const
{
    if (!m_parsed)
        const_cast<Expression*>(this)->parse(object);
    
    // 1. Safety Check
    if (m_program.empty()) return 0.0;

    // 2. Setup Variable Space
    // If varlist is null, fall back to internal storage
    const double* locals = varlist ? varlist : m_varSpace;

    // 3. Setup Stack
    // Using a raw array is fastest. 256 depth is usually sufficient for expressions.
    double stack[256];
    double* sp = stack; // Points to the next FREE slot

    // 4. Setup Instruction Pointer to the first instruction
    const Instruction* ip = m_program.data();

    // 5. The inner loop
    while (true) {

        switch (ip->code) {
        case OP_STOP:
            // Return the value sitting at the top of the stack
            // If stack is empty (shouldn't happen), return 0.0
            if (rLogicResult) *rLogicResult = (sp > stack && *(sp - 1) != 0.0);
            return (sp > stack) ? *(sp - 1) : 0.0;

            // -----------------------------------------------------
            // DATA LOADING
            // -----------------------------------------------------
        case OP_PUSH_IMM:
            *sp++ = ip->data.val;
            break;

        case OP_LOAD_LOCAL:
            // No bounds check here for speed (guaranteed by parser)
            *sp++ = locals[ip->data.index];
            break;

        case OP_LOAD_MODEL_VAR:
            // Helper call - 'const_cast' might be needed if getModelVar isn't const
            *sp++ = const_cast<Expression*>(this)->getModelVar(ip->data.index, object);
            break;

        case OP_LOAD_EXTERN_VAR:
            *sp++ = const_cast<Expression*>(this)->getExternVar(ip->data.index);
            break;

            // -----------------------------------------------------
            // ARITHMETIC (In-place modification of stack)
            // -----------------------------------------------------
        case OP_ADD:
            sp--;           // Pop
            *(sp-1) += *sp; // Add to the value below
            break;
        case OP_SUB:
            sp--;
            *(sp-1) -= *sp;
            break;
        case OP_MUL:
            sp--;
            *(sp-1) *= *sp;
            break;
        case OP_DIV:
            sp--;
            *(sp-1) /= *sp;
            break;

        case OP_POW:
            sp--;
            *(sp-1) = std::pow(*(sp-1), *sp);
            break;

        case OP_NEG:
            // Unary minus: just negate the top, don't move stack pointer
            *(sp-1) = -(*(sp-1));
            break;

            // -----------------------------------------------------
            // FUNCTIONS
            // -----------------------------------------------------
        case OP_SIN: *(sp-1) = std::sin(*(sp-1)); break;
        case OP_COS: *(sp-1) = std::cos(*(sp-1)); break;
        case OP_TAN: *(sp-1) = std::tan(*(sp-1)); break;
        case OP_EXP: *(sp-1) = std::exp(*(sp-1)); break;
        case OP_LOG: *(sp-1) = std::log(*(sp-1)); break; // Natural Log
        case OP_SQRT: *(sp-1) = std::sqrt(*(sp-1)); break;

            // -----------------------------------------------------
            // VARIADIC FUNCTIONS (Min, Max)
            // -----------------------------------------------------
        case OP_MIN: {
            int count = ip->data.count;
            // Stack has: [arg1] [arg2] ... [argN] <--- sp
            // We move sp back to [arg1]
            sp -= count;
            double m = *sp;
            // Scan forward
            for (int k = 1; k < count; ++k) {
                if (sp[k] < m) m = sp[k];
            }
            // Overwrite arg1 with result and advance sp by 1
            *sp++ = m;
            break;
        }
        case OP_MAX: {
            int count = ip->data.count;
            sp -= count;
            double m = *sp;
            for (int k = 1; k < count; ++k) {
                if (sp[k] > m) m = sp[k];
            }
            *sp++ = m;
            break;
        }

            // -----------------------------------------------------
            // LOGIC (Using 1.0 / 0.0)
            // -----------------------------------------------------
            // Compare Top (sp-1) with Below (sp-2). Pop one.
        case OP_EQ: sp--; *(sp-1) = (*(sp-1) == *sp) ? 1.0 : 0.0; break;
        case OP_NE: sp--; *(sp-1) = (*(sp-1) != *sp) ? 1.0 : 0.0; break;
        case OP_GT: sp--; *(sp-1) = (*(sp-1) > *sp)  ? 1.0 : 0.0; break;
        case OP_LT: sp--; *(sp-1) = (*(sp-1) < *sp)  ? 1.0 : 0.0; break;
        case OP_GE: sp--; *(sp-1) = (*(sp-1) >= *sp) ? 1.0 : 0.0; break;
        case OP_LE: sp--; *(sp-1) = (*(sp-1) <= *sp) ? 1.0 : 0.0; break;

        case OP_AND:
            sp--;
            // Standard C++ bool cast: anything != 0 is true
            *(sp-1) = (*(sp-1) != 0.0 && *sp != 0.0) ? 1.0 : 0.0;
            break;
        case OP_OR:
            sp--;
            *(sp-1) = (*(sp-1) != 0.0 || *sp != 0.0) ? 1.0 : 0.0;
            break;

        case OP_IF:
            // Structure: if(condition, val_true, val_false)
            // Stack: [Condition] [TrueVal] [FalseVal] <--- sp
            sp -= 3;
            // If condition != 0, pick TrueVal (offset 1), else FalseVal (offset 2)
            *sp = (*sp != 0.0) ? *(sp+1) : *(sp+2);
            sp++;
            break;

        // -----------------------------------------------------
        // SPECIAL CUSTOM FUNCTIONS
        // -----------------------------------------------------
        case OP_INCSUM:
            m_incSumVar += *(sp-1);
            *(sp-1) = m_incSumVar;
            break;

        case OP_MODULO:
            sp--;
            *(sp-1)=fmod(*(sp-1), *sp);
            break;


        case OP_RND:
            sp--;
            *(sp-1) = udfRandom(0, *(sp-1), *sp);
            break;

        case OP_RNDG:
            sp--;
            *(sp-1) = udfRandom(1, *(sp-1), *sp);
            break;

        case OP_ROUND:
            *(sp-1) = std::floor(*(sp-1) + 0.5);
            break;

        case OP_SIGMOID:
            sp-=3;
            *(sp-1) = udfSigmoid(*(sp-1), *sp, *(sp+1), *(sp+2));
            break;

        case OP_POLYGON: {
            int count = ip->data.count;
            double x = *(sp - count);
            double result = udfPolygon(x, sp-1, count);

            sp-= count;
            *sp++ = result;
            break;
        }
        case OP_IN: {
            // Stack: [Val] [L1] [L2] ... [Ln] <--- sp
            int count = ip->data.count;
            double val = *(sp - count);

            // udfIn receives Stack which is 'sp - 1' (last arg, vn)
            // ArgCount is total number of arguments (e.g. 3 for in(x, v1, v2))
            // Value is the first argument (x)
            double result = udfIn(val, sp - 1, count);
            sp -= count; // pop all args
            *sp++ = result; // push result
            break;
        }
        
        case OP_LIMIT: {
            // limit(value, min, max)
            double val = *(sp-3);
            double minv = *(sp-2);
            double maxv = *(sp-1);
            if (minv > maxv) std::swap(minv, maxv);
            *(sp-3) = std::max(minv, std::min(val, maxv));
            sp-=2;
            break;
        }
        
        // SVD Specific
        case OP_LOCALNB: {
            int count = ip->data.count;
            *(sp - count) = udfNeighborhood(object, 1, sp-1, count);
            sp -= (count - 1);
            break;
        }
        case OP_INTERMEDIATENB: {
            int count = ip->data.count;
            *(sp - count) = udfNeighborhood(object, 2, sp-1, count);
            sp -= (count - 1);
            break;
        }
        case OP_GLOBALNB: {
            int count = ip->data.count;
            *(sp - count) = udfNeighborhood(object, 3, sp-1, count);
            sp -= (count - 1);
            break;
        }
        case OP_DISTANCE: {
            int count = ip->data.count;
            *(sp - count) = udfNeighborhood(object, 4, sp-1, count);
            sp -= (count - 1);
            break;
        }
        case OP_SPECIESPROPORTION: {
            int count = ip->data.count;
            *(sp - count) = udfSpeciesProportion(object, sp-1, count);
            sp -= (count - 1);
            break;
        }


        default:
            throw std::logic_error("invalid token during (compiled) execution: " + m_expression);
        }

        // Move to next instruction
        ip++;
    }
}

double * Expression::addVar(const std::string& VarName)
{
    // add var
    int idx=index_of(m_varList, VarName);
    if (idx==-1) {
        m_varList.push_back(VarName);
    }
    return &m_varSpace[getVarIndex(VarName)];
}

double *  Expression::getVarAdress(const std::string& VarName)
{
    if (!m_parsed)
        parse();
    int idx=getVarIndex(VarName);
    if (idx>=0 && idx<MAXLOCALVAR)
        return &m_varSpace[idx];
    else
        throw std::logic_error("Expression::getVarAdress: Invalid variable <"+ VarName + "> ");
}

int  Expression::getVarIndex(const std::string& variableName)
{
    int idx;

    if (mModelObject) {
        idx = mModelObject->variableIndex(variableName); // was lowercase(variableName) - I think we are strict in SVD
        if (idx>-1)
            return 100 + idx;
    }
    if (mScriptIndexFunc) {
        idx = (*mScriptIndexFunc)(variableName);
        if (idx>-1)
            return 1000 + idx;
    }

    // external variables
    if (!(m_externVarNames.size()==0))
    {
        idx=index_of(m_externVarNames, variableName);
        if (idx>-1)
            return 1000 + idx;
    }
    idx = index_of(m_varList, variableName);
    if (idx>-1)
        return idx;
    // if in strict mode, all variables must be already available at this stage.
    if (m_strict) {
        m_errorMsg = "Variable '" + variableName + "' in (strict) expression '" + m_expression + "' not available!";
        if (!m_catchExceptions)
            throw std::logic_error(m_errorMsg);
   }
    return -1;
}

inline double Expression::getModelVar(const int varIdx, ExpressionWrapper *object) const
{

    ExpressionWrapper *model_object = object?object:mModelObject;
    int idx=varIdx - 100; // saved as 100+x
    if (model_object)
        return model_object->value(idx);
    throw std::logic_error("Expression::getModelVar: invalid modell variable!");

}

void Expression::setExternalVarSpace(const std::vector<std::string>& ExternSpaceNames, double* ExternSpace)
{
    m_externVarSpace=ExternSpace;
    m_externVarNames=ExternSpaceNames;
}

double Expression::getExternVar(int Index) const
{
    if (mScriptValueFunc)
        return (*mScriptValueFunc)(Index-1000);
    else
        return m_externVarSpace[Index-1000];
}

void Expression::enableIncSum()
{
    m_incSumEnabled=true;
    m_incSumVar=0.;
}


// "Userdefined Function" Polygon
double  Expression::udfPolygon(double Value, double* Stack, int ArgCount) const
{
    // the stack contains points (pairs of x/y) which define a line
    // return value is y=f(x)
    // Note: *Stack points to the last argument (y-coord of last point)
    if (ArgCount%2!=1)
        throw std::logic_error("Expression::polygon: wrong number of arguments. polygon(<val>, x0, y0, x1, y1, ....)");
    int PointCnt = (ArgCount-1) / 2;
    if (PointCnt<2)
        throw std::logic_error("Expression::polygon: wrong number of arguments. polygon(<val>, x0, y0, x1, y1, ....)");
    double x,y, xold, yold;
    y=*Stack--;   // 1. argument
    x=*Stack--;
    if (Value>x)
        return y;
    for (int i=0; i<PointCnt-1; i++) {
        xold=x;
        yold=y;
        y=*Stack--;   // x,y-pair from stack
        x=*Stack--;
        if (Value>x) {
            return (yold-y)/(xold-x) * (Value-x) + y;
        }

    }
    // x smaller than the first point
    return y;
}

// userdefined func sigmoid....
double Expression::udfSigmoid(double Value, double sType, double p1, double p2) const
{
    // sType: typ of function:
    // 0: logistic f
    // 1: Hill-function
    // 2: 1 - logistic (1 to 0)
    // 3: 1- hill
    double Result;

    double x=std::max(std::min(Value, 1.), 0.);  // limit to [0..1]
    int typ=(int) sType;
    switch (typ) {
         case 0: case 2: // logistic function: f(x)=1 / (1 + p1 e^(-p2 * x))
                     Result=1. / (1. + p1 * exp(-p2 * x));
             break;
         case 1: case 3:     // Hill-function: f(x)=(x^p1)/(p2^p1+x^p1)
                     Result=pow(x, p1) / ( pow(p2,p1) + pow(x,p1));
             break;
         default:
             throw std::logic_error("sigmoid-function: invalid type of function: 0..3");
         }
    if (typ==2 || typ==3)
        Result=1. - Result;

    return Result;
}

double Expression::udfIn(double Value, double *Stack, int ArgCount) const
{
    // signature: in(x, v1, v2, v3, ..., vn)
    if (ArgCount<2)
        throw std::logic_error("Expression: in() function: not enough parameters");
    
    // Stack points to the last argument (vn).
    // We check against v1...vn (ArgCount-1 values).
    double *p = Stack;
    for (int i=0; i<ArgCount-1; ++i) {
        if (*p == Value) return 1.0; // True
        p--;
    }
    return 0.0; // False
}


// SVD specific neighborhood functions
double Expression::udfNeighborhood(ExpressionWrapper *object, int neighbor_class, double *Stack, int ArgCount) const
{
    // signature: f(
    if (!object) return 0.;
    CellWrapper *wrap = dynamic_cast<CellWrapper*>(object);
    if (!wrap) return 0.;
    double *p = Stack - (ArgCount-1);
    double result = 0.;
    if (neighbor_class == 4)
        result = 1000000; // cells
    while (p <= Stack) {
        size_t stateId = static_cast<size_t>( *p );
        switch (neighbor_class) {
        case 1: result += wrap->localStateAverage(stateId); break;
        case 2: result += wrap->intermediateStateAverage(stateId); break;
        case 3: result +=  wrap->globalStateAverage(stateId); break;
        case 4: result = std::min(result, wrap->minimumDistanceTo(stateId)); break;
        default: break;
        }
        ++p;
    }
    return result;
}

double Expression::udfSpeciesProportion(ExpressionWrapper *object, double *Stack, int ArgCount) const
{
    if (!object) return 0.;
    CellWrapper *wrap = dynamic_cast<CellWrapper*>(object);
    if (!wrap) return 0.;
    double *p = Stack - (ArgCount-1);
    double result = 0;
    while (p <= Stack) {
        size_t species_index = static_cast<size_t>( *p );
        result += wrap->speciesProportion(species_index);
        ++p;
    }
    return result;
}


void Expression::checkBuffer(int Index)
{
    // manage the buffer: increase size if necessary
    if (Index<m_execListSize)
        return;
    int NewSize=m_execListSize * 2; // double size every time: 5->10->20->40->80->160
    // (1) create new buffer
    ExtExecListItem *NewBuf=new ExtExecListItem[NewSize];
    // (2) copy values
    for (int i=0;i<m_execListSize;i++)
        NewBuf[i]=m_execList[i];
    // (3) use new buffer
    delete[] m_execList;
    m_execList = NewBuf;
    m_execListSize=NewSize;
}


double Expression::udfRandom(int type, double p1, double p2) const
{
    // random
    if (type == 0)
        return nrandom(p1, p2);
    else    // gaussian
        //return mtRand().randNorm(p1, p2);
        throw std::logic_error("udfRandom: gaussian random number not implemented");
}

/** Linarize an expression, i.e. approximate the function by linear interpolation.
    This is an option for performance critical calculations that include time consuming mathematic functions (e.g. exp())
    low_value: linearization start at this value. values below produce an error
    high_value: upper limit
    steps: number of steps the function is split into
  */
void Expression::linearize(const double low_value, const double high_value, const int steps)
{
    if (!mLinearizationAllowed)
        return;

    mLinearized.clear();
    mLinearLow = low_value;
    mLinearHigh  = high_value;
    mLinearStep = (high_value - low_value) / (double(steps));
    mLinearInvStep = 1. / mLinearStep;
    for (int i=0;i<=steps+1;i++) { // Add one extra step for safety/interpolation at high end
        double x = mLinearLow + i*mLinearStep;
        double r = calculate(x);
        mLinearized.push_back(r);
    }
    mLinearizeMode = 1;
}

/// like 'linearize()' but for 2d-matrices
void Expression::linearize2d(const double low_x, const double high_x,
                             const double low_y, const double high_y,
                             const int stepsx, const int stepsy)
{
    if (!mLinearizationAllowed)
        return;
    mLinearized.clear();
    mLinearLow = low_x;
    mLinearHigh  = high_x;
    mLinearLowY = low_y;
    mLinearHighY = high_y;

    mLinearStep = (high_x - low_x) / (double(stepsx));
    mLinearInvStep = 1. / mLinearStep;
    mLinearStepY = (high_y - low_y) / (double(stepsy));
    mLinearInvStepY = 1. / mLinearStepY;
    
    for (int i=0;i<=stepsx+1;i++) {
        for (int j=0;j<=stepsy+1;j++) {
            double x = mLinearLow + i*mLinearStep;
            double y = mLinearLowY + j*mLinearStepY;
            double r = calculate(x,y);
            mLinearized.push_back(r);
        }
    }
    mLinearStepCountY = stepsy + 2; // +2 because we added +1 to stepsy loop? No, stepsy+1 points + 1 extra?
    // In loop: j goes 0..stepsy+1. Total points: stepsy+2.
    
    mLinearizeMode = 2;

}
