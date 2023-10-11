# Fire module

The SVD fire module simulates wildfire on 100 m resolution (the SVD base resolution). Fire spreads dynamically with a cellular automaton approach and considers wind conditions, terrain, and burn probabilities specified for each vegetation state. A fire keeps spreading until it stops intrinsically (e.g., when the combustible biomass is exhausted and no further spread is possible), or when a pre-defined maximum fire size is reached. Fire ignition processes are not explicitly simulated in SVD, but rely on a list of predefined ignitions. Multiple fires can start in any given year, and a fire is defined by starting location, maximum fire size, and wind conditions (direction, speed).

The implementation of fire spread is done in rounds. In each round, first the probability of spreading to neighboring cells is calculated for each cell currently on fire (Moore-Neighborhood) and accumulated for potential target cells (i.e., the probability is higher if several neighboring cells are currently burning). In a second pass, for each target cell fire reaches the cell with the accumulated probability from the previous step, and consumes the cell with a state specific burn probability. After burning, fire may spread further in the next round, unless the fire extinguishes on the focal cell (with probability *p~ext~*). The process stops when the burned area reaches the pre-defined maximum fire size for the event, or when within one round no new cells are ignited. In order to avoid the risk of a fire stopping due to chance very early, the state specific burn probability is assumed to be 1 and no fire extinguishing is simulated (i.e. *p~ext~* set to 0) during the first five rounds (but note that the maximum fire size is still enforced).

*Spread probability*

The probability of fire spread from a burning cell to one of its neighboring cells is based on the approach of FireBGC v2 (Keane et al., 2011) and the specific implementation in iLand (Seidl et al., 2014). The fire spread distance *d~spread~* (m) is influenced by wind speed and wind direction as well as the slope on the DEM, with fires spreading further upslope and downwind (Keane et al., 2011). Since fire can only spread from cell to cell, *d~spread~* is transformed into the probability of reaching the focus cell *p~spread~* (Eq. S3-1).

|                                                     |            |
|-----------------------------------------------------|------------|
| $p_{spread}=p_{dist}^{1\over{d_{spread}/d_{cell}}}$ | (Eq. S3-1) |

with *d~cell~* the distance to the focus cell (100m for immediate neighbors, and 141.42m for diagonal neighbors), and *p~dist~* a parameter specifying the probability that the fire reaches a distance of *d~spread~* when it would have to spread across multiple cells. Fire spotting is currently not implemented.

### Fire specific state attributes

The fire module requires module-specific attributes for each state in SVD. These attributes are defined by a table defined with the `stateFile` setting. The table must have the following columns:

| Attribute | Description                                                                    |
|-----------|--------------------------------------------------------------------------------|
| pSeverity | probability of a high-severity burn.                                           |
| pBurn     | probability [0..1] that a cell in the current state burns when reached by fire |

The post-fire transition is defined by the transition matrix in `transitionFile`. Note that different transitions can be defined for low- and high-severity fire. High severity is encoded by `key=1` in the matrix (see [matrix module](module_matrix.md) for details).

### Ignitions

Fires are not started by SVD, but are triggered externally by a time series of fires (`ignitionFile`). The file has the following columns:

| Column        | Description                                                                                                                       |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------|
| year          | Simulation year of the ignition. The first year is 1, the second year 2, etc. Not related to the year of the climate time series. |
| x             | x-coordinate of the ignition point in local metric coordinates                                                                    |
| y             | y-coordinate of the ignition point                                                                                                |
| max_size      | maximum fire size in ha                                                                                                           |
| windspeed     | wind speed (m/s)                                                                                                                  |
| winddirection | wind direction in degrees (0 = north, 90=east, ...)                                                                               |
| id            | unique id of the fire event (not mandatory)                                                                                       |

: Fire ignition file

## Configuration

The module is configured in the [project file](project_file.md).\
In addition to the `enabled` and `type` setting, the fire module has the following settings:

-   

    ### `transitionFile` (filepath)

    The file containing a transition matrix (see [matrix module](module_matrix.md)) for post fire states (see above).

-   

    ### `ignitionFile` (filepath)

    The data table with the list of ignition points and fire sizes.

-   

    ### `stateFile` (filepath)

    Data file with additional state-specific variables that specify per-state burn probabilities.

-   

    ### `extinguishProb` (numeric)

    Probability that the fire does not spread after burning a cell.

-   ### `spreadDistProb` (numeric)

    The parameter *p~dist~* used in the calculation of fire spread probabilities (see text above). Higher values translate to higher probabilities of fire spread from one cell to another.

-   

    ### `fireSizeMultiplier` (expression)

if not empty, the expression is used to scale the maximum fire size provided from the ignition file. Many fire do not reach the maximum size because of topography (lack of contiguous flammable area) or other reasons. This can be countered by scaling fire sizes, so that on average the realized fire sizes match the input. The expression takes one parameter, the fire size from the ignition file.

## Output

The module can provide tabular and gridded [output](outputs.md#Fire).
