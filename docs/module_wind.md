# Wind module

The SVD wind module simulates windthrows on 100 m resolution (the SVD base resolution). Wind spreads dynamically.

## Module implementation

The SVD wind module simulates the effects of storm events on the landscape, incorporating statistically derived event characteristics and a detailed representation of forest susceptibility. For each storm event, SVD first determines the affected 10 km grid cell based on the location of the storm, and dynamically grows the storms’ footprint for larger storm events by sampling adjacent 10km cells based on their spatial occurrence probability.

It then simulates the detailed spread of the storm within this region at 100 m resolution. Within a 10 km region, the most susceptible 100 m cells are selected as starting points for the spread simulation. Susceptibility is hereby dynamically updated between iterations based on the present forest state. Starting from the most susceptible cells, the simulated disturbance spreads using a neighborhood-based approach.

The probability of a neighboring cell being affected is influenced by:

-   Cell susceptibility, with highly susceptible cells more likely to be impacted.
-   Proximity to impacted cells, as cells adjacent to previously affected cells have an increased risk.
-   Parameters that control the degree to which the disturbance can spread into less susceptible cells and across undisturbed areas.

The simulated storm event continues to spread until it reaches its statistically determined size or if suitable areas for further spread are exhausted within the 10 km region. When a cell is impacted by the simulated storm, its forest state changes according to a transition matrix. The module assumes high-severity disturbance on 100m cells and a post-event species composition that resembles the pre-storm state.

At the cell level, the windthrow probability is defined by the state and and modified by the neighborhood. There are a number of parameters whose interplay affect the resulting spatial patterns. The parameter `fetchFactor` defines how much the impact probability of a cell increases if an adjacent cell is already affected, representing increased susceptibility due to gaps created during the wind even. The windthrow impact may stop spreading after impacting a cell (`stopAfterImpact`). The parameter `spreadUndisturbed` defines the probability that spread continues even if a cell is *not* affected (i.e., "hopping" over undisturbed cells). By default, the spread starts at the most susceptible cells. The parameter `spreadStartParallel` can be used to increase the number of starting points, which leads to a more homogenous and less clustered windthrow disturbance.

## Wind specific state attributes

The wind module requires module-specific attributes for each state in SVD. These attributes are defined by a table defined with the `stateFile` setting. The table must have the following columns:

| Attribute   | Description                                                                           |
|:-----------------------------------|:-----------------------------------|
| stateId     | Id of SVD state                                                                       |
| pWindDamage | Probability [0..1] that a cell in the current state is affected when reached by storm |

The post-wind transition is defined by the transition matrix in `transitionFile`.

## Storm events

Windstorms are not started by SVD, but are triggered externally by a time series of storms (`stormEventFile`). The file has the following columns:

| Column             | Description                                                                                  |
|:-----------------------------------|:-----------------------------------|
| year               | Year of storm occurrence                                                                     |
| x                  | X-coordinate of the storm event in local metric coordinates (projection used by the project) |
| y                  | Y-coordinate of the storm event                                                              |
| number_of_cells    | Number of 10km cells affected                                                                |
| proportion_of_cell | Proportion of affected 100m cells affected within 10km                                       |

## Configuration

The module is configured in the [project file](project_file.md). In addition to the `enabled` and `type` setting, the wind module has the following settings:

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

    Probability that the wind does not spread after affecting a cell (0..1)

-   

    ### `spreadUndisturbed` (numeric)

    The parameter defines if windthrows can continue after a cell is not affected (0..1)

-   

    ### `fetchFactor` (numeric)

    The parameter defines how much the probability increases for neighboring cells.

-   

    ### `spreadStartParallel` (numeric)

    The parameter defines whether wind throws start in parallel in different areas of the 10km grid.

    Can be used to change the number of small scale windthrows.

-   

    ### `windSizeMultiplier` (expression)

    If not empty, the expression is used to scale the maximum wind size provided from the storm event file.

    Many storms do not reach the maximum size because of topography and lack of continuously forested patches. This can be countered by scaling storm sizes, so that on average the realized storm event sizes match the input. The expression takes one parameter, the `proportion_of_cell` from the storm event file.

## Output

The module can provide tabular and gridded [outputs.](outputs.md#Wind)
