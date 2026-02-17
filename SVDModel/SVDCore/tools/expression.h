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

#ifndef expressionH
#define expressionH

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <cassert>
#include <cstring> // for memset

#define MAXLOCALVAR 15
class ExpressionWrapper;
class Expression
{
public:
        ~Expression();
        Expression();
        enum BoolValue { False=0, True=1 };
        Expression(const std::string &aExpression) { setExpression(aExpression); }
        Expression(const std::string &expression, ExpressionWrapper *wrapper) { setExpression(expression); mModelObject = wrapper;  }

        // intialization
        void setExpression(const std::string &aExpression); ///< set expression
        void setAndParse(const std::string &expr); ///< set expression and parse instantly
        void setModelObject(ExpressionWrapper *wrapper) { mModelObject = wrapper; }
        const std::string &expression() const { return m_expression; }
        void  parse(ExpressionWrapper *wrapper=nullptr); ///< force a parsing of the expression
        void linearize(const double low_value, const double high_value, const int steps=1000);
        void linearize2d(const double low_x, const double high_x, const double low_y, const double high_y, const int stepsx=50, const int stepsy=50);
        static void setLinearizationEnabled(const bool enable) {mLinearizationAllowed = enable; }
        /// access from external scripting (e.g. Picus Script engine)
        /// @param get_index a function with the signature int func(const std::string &var_name) -> returns the index of the variable
        /// @param get_value a function with the signature double func(int var_index) --> to retrieve the current value of the associated function
        void setScriptingFunctions( int (*get_index)(const std::string &), double (*get_value)(int)) { mScriptIndexFunc=get_index; mScriptValueFunc = get_value; }


        // calculations
        /// calculate formula and return result. variable values need to be set using "setVar()"
        double execute(double *varlist=0, ExpressionWrapper *object=0, bool *rLogicResult=0) const;
        double executeLocked() { std::lock_guard<std::mutex> m(m_execMutex); return execute();  } ///< thread safe version

        /** calculate formula. the first two variables are assigned the values Val1 and Val2. This function is for convenience.
           the return is the result of the calculation.
           e.g.: x+3*y --> Val1->x, Val2->y
           forceExecution: do not apply linearization */
        double calculate(const double Val1=0., const double Val2=0., const bool forceExecution=false) const;
        bool calculateBool(const double Val1=0., const double Val2=0., const bool forceExecution=false) const {
            double res = calculate(Val1, Val2, forceExecution);
            return !(res==0.);
        }

        /// calculate formula with object
        ///
        double calculate(ExpressionWrapper &object, const double variable_value1=0., const double variable_value2=0.) const;
        bool calculateBool(ExpressionWrapper &object, const double variable_value1=0., const double variable_value2=0.) const {
            double res = calculate(object, variable_value1, variable_value2);
            return !(res==0.);
        }

        //variables
        /// set the value of the variable named "Var". Note: using addVar to obtain a pointer may be more efficient for multiple executions.
        void  setVar(const std::string& Var, double Value);
        /// adds variable "VarName" and returns a double pointer to the variable. Use *ptr to set the value (before calling execute())
        double *addVar(const std::string& VarName);
        /// retrieve again the value pointer of a variable.
        double *  getVarAdress(const std::string& VarName);


        bool isConstExpression() const { return m_constExpression; } ///< returns true if current expression is a constant.
        bool isEmpty() const { return m_empty; } ///< returns true if expression is empty
        const std::string &lastError() const { return m_errorMsg; }
        /** strict property: if true, variables must be named before execution.
          When strict=true, all variables in the expression must be added by setVar or addVar.
          if false, variable values are assigned depending on occurence. strict is false by default for calls to "calculate()".
        */
        bool isStrict() { return m_strict;}
        void setStrict(bool str) { m_strict=str; }
        void setCatchExceptions(bool docatch=true) { m_catchExceptions = docatch; }
        void setExternalVarSpace(const std::vector<std::string>& ExternSpaceNames, double* ExternSpace);
        void enableIncSum();

        static void setConstants(const std::vector<std::string> &consts);

private:
        enum ETokType {etNumber, etOperator, etVariable, etFunction, etLogical, etCompare, etStop, etUnknown, etDelimeter};
        enum EValueClasses {evcBHD, evcHoehe, evcAlter};

        enum OpCode : uint8_t {
            OP_STOP = 0,
            OP_PUSH_IMM,    // Push Immediate Value
            OP_LOAD_LOCAL,  // Load from varlist (Stack variables 0..99)
            OP_LOAD_MODEL_VAR,  // Calls getModelVar(index, object)
            OP_LOAD_EXTERN_VAR, // Calls getExternVar(index)


            // Arithmetic
            OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_NEG,

            // Functions
            OP_SIN, OP_COS, OP_TAN, OP_EXP, OP_LOG, OP_SQRT,
            OP_MIN, OP_MAX, OP_AVG, // Variadic

            // Logic (Using Double as Bool: 1.0=True, 0.0=False)
            OP_AND, OP_OR,
            OP_GT, OP_LT, OP_GE, OP_LE, OP_EQ, OP_NE,
            OP_IF,
            // custom functions
            OP_INCSUM, OP_POLYGON, OP_MODULO, OP_SIGMOID, OP_RND, OP_RNDG, OP_LIMIT, OP_ROUND, OP_IN,
            // SVD specific
            OP_LOCALNB, OP_INTERMEDIATENB, OP_GLOBALNB, OP_DISTANCE, OP_SPECIESPROPORTION
        };

        struct Instruction {
            OpCode code;
            // Padding to ensure union alignment (usually 8 bytes total for the top half)
            uint8_t _pad[7];

            union {
                double  val;        // For literals
                double* ptr;        // For global variables (resolved address)
                int     index;      // For local variables (index in varlist)
                int     count;      // For variadic functions (min/max)
            } data;
        };
        // optimized data structure for fast execution (cache aware, avoid pointer ops)
        std::vector<Instruction> m_program;
        void compile(); // The translator from the old execution list to the fast program

        struct ExtExecListItem {
            ETokType Type;
            double  Value;
            int     Index;
        };
        enum EDatatype {edtInfo, edtNumber, edtString, edtObject, edtVoid, edtObjVar, edtReference, edtObjectReference};
        bool m_catchExceptions;
        std::string m_errorMsg;

        bool m_parsed;
        mutable bool m_strict;
        bool m_empty; // empty expression
        bool m_constExpression;
        std::string m_tokString;
        std::string m_expression;
        Expression::ExtExecListItem *m_execList;
        int m_execListSize; // size of buffer
        int m_execIndex;
        double m_varSpace[MAXLOCALVAR];
        std::vector<std::string> m_varList;
        std::vector<std::string> m_externVarNames;
        double *m_externVarSpace;
        Expression::ETokType m_state;
        Expression::ETokType m_lastState;
        char *m_pos;
        char *m_expr;
        std::string m_token;
        std::string m_prepStr;
        int   m_tokCount;
        Expression::ETokType  next_token();
        void  atom();
        void  parse_levelL0();
        void  parse_levelL1();
        void  parse_level0();
        void  parse_level1();
        void  parse_level2();
        void  parse_level3();
        void  parse_level4();
        int  getFuncIndex(const std::string& functionName);
        int  getVarIndex(const std::string& variableName);
        inline double getModelVar(const int varIdx, ExpressionWrapper *object=0) const ;

        // link to external model variable
        ExpressionWrapper *mModelObject;

        double getExternVar(const int Index) const;
        // inc-sum
        mutable double m_incSumVar;
        bool   m_incSumEnabled;

        static std::vector<std::string> mConstants;
        // user defined function, SVD specific

        double udfRandom(int type, double p1, double p2) const; ///< user defined function rnd() (normal distribution does not work now!)

        double  udfPolygon(double Value, double* Stack, int ArgCount) const; ///< special function polygon()
        double udfSigmoid(double Value, double sType, double p1, double p2) const; ///< special function sigmoid()
        double  udfIn(double Value, double* Stack, int ArgCount) const; ///< special function in()
        // SVD specific neighborhood functions
        double udfNeighborhood(ExpressionWrapper *object, int neighbor_class, double *Stack, int ArgCount) const;
        // SVD specifc functions acessing species part of a state
        double udfSpeciesProportion(ExpressionWrapper *object, double *Stack, int ArgCount) const;

        void checkBuffer(int Index);
        
        // Helper struct to make Expression copyable/movable despite having a mutex
        struct CopyableMutex : std::mutex {
            CopyableMutex() = default;
            CopyableMutex(const CopyableMutex&) noexcept : std::mutex() {}
            CopyableMutex(CopyableMutex&&) noexcept : std::mutex() {}
            CopyableMutex& operator=(const CopyableMutex&) noexcept { return *this; }
            CopyableMutex& operator=(CopyableMutex&&) noexcept { return *this; }
        };
        mutable CopyableMutex m_execMutex; // mutable to allow locking in const methods

        // linearization
        inline double linearizedValue(const double x) const {
            if (x<mLinearLow || x>mLinearHigh)
                return calculate(x,0.,true); // standard calculation without linear optimization- but force calculation to avoid infinite loop

            double pos = (x - mLinearLow) * mLinearInvStep;

            int lower = (int)pos;
            double delta = pos - lower; // fractional part (0.0 to 1.0)

            assert(lower+1<mLinearized.size());

            // 4. Access data
            const double* entry = &mLinearized[lower];

            // 5. Lerp (Linear Interpolation)
            // Formula: y0 + (y1 - y0) * delta
            double result = *entry + (*(entry + 1) - *entry) * delta;
            return result;
        }
        inline double linearizedValue2d(const double x, const double y) const {
            // 1. Bounds Check
            if (x < mLinearLow || x > mLinearHigh || y < mLinearLowY || y > mLinearHighY)
                return calculate(x, y, true);

            // 2. Calculate normalized position (remove expensive divisions)
            // "pos" is the index with a fractional part (e.g., 5.42)
            double posX = (x - mLinearLow) * mLinearInvStep;
            double posY = (y - mLinearLowY) * mLinearInvStepY;

            // 3. Split into Integer Index and Fractional Weight
            // Fast cast to int (truncation)
            int xi = (int)posX;
            int yi = (int)posY;

            // The weights (0.0 to 1.0)
            double tx = posX - xi;
            double ty = posY - yi;

            // 4. Calculate Memory Index
            // Your layout has Y varying fast, so we jump 'mLinearStepCountY' to get to next X
            int idx = xi * mLinearStepCountY + yi;

            // 5. Fetch the 4 corners
            // Pointers are faster than repeated vector[] lookups
            const double* pBase = &mLinearized[idx];

            // Layout:
            // [idx]   = x0, y0
            // [idx+1] = x0, y1 (Next Y)
            // [idx+S] = x1, y0 (Next X)
            double v00 = *pBase;
            double v01 = *(pBase + 1);
            double v10 = *(pBase + mLinearStepCountY);
            double v11 = *(pBase + mLinearStepCountY + 1);

            // 6. Bilinear Interpolation (Lerp)
            // Interpolate along Y axis first
            double col0 = v00 + ty * (v01 - v00);
            double col1 = v10 + ty * (v11 - v10);

            // Interpolate along X axis
            return col0 + tx * (col1 - col0);
        }
        int mLinearizeMode;
        std::vector<double> mLinearized;
        double mLinearLow, mLinearHigh;
        double mLinearStep, mLinearInvStep;
        double mLinearLowY, mLinearHighY;
        double mLinearStepY, mLinearInvStepY;
        int mLinearStepCountY;
        static bool mLinearizationAllowed;
        // access to scripting...
        int (*mScriptIndexFunc)(const std::string &);
        double (*mScriptValueFunc)(int);
};

#endif   // EXPRESSIONH