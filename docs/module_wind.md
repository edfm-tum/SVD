# Wind module

The SVD wind module simulates windthrows on 100 m resolution (the SVD base resolution). Wind spreads dynamically...


*Spread probability*


### Wind specific state attributes

The wind module requires module-specific attributes for each state in SVD. These attributes are defined by a table defined with the `stateFile` setting. The table must have the following columns:

| Attribute | Description                                                                          |
|-----------|--------------------------------------------------------------------------------------|
| stateId   | Id of svd state						                                         	   |
| pDamage   | probability [0..1] that a cell in the current state is affected when reached by storm|

The post-wind transition is defined by the transition matrix in `transitionFile`. 


### Storm events

Windstorms are not started by SVD, but are triggered externally by a time series of storms (`stormEventFile`). The file has the following columns:

| Column        | Description                                                       |
|---------------|-------------------------------------------------------------------|
| cell_position | Cell id                                                           |
| x             | x-coordinate of the ignition point in local metric coordinates    |
| y             | y-coordinate of the ignition point                                |
| number_of_cells      | number of 10km cells affected                              |
| proportion of cell   | proportion of affected 100m cells affected within 10km     |
| year | year of storm occurrence                                                   |



## Configuration

The module is configured in the [project file](project_file.md).\
In addition to the `enabled` and `type` setting, the wind module has the following settings:


-   

    ### `regionalProbabilityGrid` (filepath)

    The file containing a the 10km probability of wind occurrence.
    
-   

    ### `transitionFile` (filepath)

    The file containing a transition matrix (see [matrix module](module_matrix.md)) for post wind states (see above).

-   

    ### `ignitionFile` (filepath)

    The data table with the list of event location points and wind sizes.

-   

    ### `stateFile` (filepath)

    Data file with additional state-specific variables that specify per-state damage probabilities.

-   

    ### `stopAfterImpact` (numeric)

    Probability that the wind does not spread after affecting a cell.
    
    
-  
    ### `spreadUndisturbed` (numeric)
    
    The parameter defines if windthrows can continue after a cell is not affected.
    

-   
    ### `fetchFactor` (numeric)
    
    The parameter defines how much the probability increases for neighboring cells.
     
    
-    
    ### `spreadStartParallel` (numeric)
    
    The parameter defines whether wind throws start in parallel in different areas of the 10km grid. 
    Can be used to change the number of small scale windthrows

-   

    ### `windSizeMultiplier` (expression)

if not empty, the expression is used to scale the maximum wind size provided from the storm event file. Many storms do not reach the maximum size because of topography. This can be countered by scaling storm sizes, so that on average the realized storm event sizes match the input. The expression takes one parameter, the proportion from the storm event file.


## Output

The module can provide tabular and gridded [output](outputs.md#Wind).
