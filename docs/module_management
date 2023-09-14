# Management module

The SVD management module simulates management actions on 100 m resolution (the SVD base resolution). Management is performed as clear cut of the grid cell. Management happens between minH and maxH which can be defined by parameters (see below). Further, management probabilities are calculated externally and given to SVD with the transitionFile. In order to prevent from large scale clearcuts in year 1, there is a burn-in period that can be defined via expression. 



### Management actions

Management is triggered when height growth increment is below a certain threshold


| Column        | Description                                                       |
|---------------|-------------------------------------------------------------------|
| stateId       				| SVD state                                         						|
| heightIncrementThreshold      | externaly defined threshold                        						|
| pManagement   				| probability of management after crossing the heightIncrementThreshold     |



## Configuration

The module is configured in the [project file](project_file.md).\
In addition to the `enabled` and `type` setting, the management module has the following settings:


-
    ### `transitionFile` (filepath)

    The file containing a transition matrix (see [matrix module](module_matrix.md)) for post management states (see above).

-   

    ### `stateFile` (filepath)

    Data file with additional state-specific variables that specify per-state damage probabilities.

-   

    ### `AutoManagement.minHeight` (numeric)

    Height after which management becomes possible
    
-   

    ### `AutoManagement.burnInProbability` (expression)

if not empty, the expression is used to scale the management actions that would happen in the first year to several years afterards.
example: polygon(yr, 0, 0, 1, 0, 1, 0.5, 10, 1); from year 0 to 1 the probability of management is 0. From year 1 the management probability increases from 0.5 to 1. 


## Output

The module can provide tabular and gridded [output](outputs.md#Management).
