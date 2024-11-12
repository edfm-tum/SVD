# Management module

The SVD management module simulates management actions on 100 m resolution (the SVD base resolution). Management is performed as clear cut of the grid cell. Management happens for stands above *minHeight* and whose annual height increment is below a (state-specific) threshold. The later mimics the tree growth & yield principle trees are harvested after growth has culminated (and is declining). Further, management probabilities are calculated externally and given to SVD with the `transitionFile`. In order to prevent large scale clearcuts in year 1, there is a burn-in period that can be defined via an expression.

### Management actions

Management is triggered when:

-    height growth increment is below the state-specific threshold, and height is above `minHeight`

-   regionally managed area has not yet reached the maximum in the current year (management cap)

-   with a probability of state-specific probability \* burn-in factor (`pManagement` \* `burnInProbability`)

| Column                   | Description                                                                                              |
|----------------------|--------------------------------------------------|
| stateId                  | SVD state                                                                                                |
| heightIncrementThreshold | externaly defined threshold; management only if recent height increment (m/year) is below that threshold |
| pManagement              | probability of management after crossing the `heightIncrementThreshold`                                  |

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

    ### `minHeight` (numeric)

    Height after which management becomes possible (meters)

-   

    ### `burnInProbability` (expression)

if not empty, the expression is used to scale the management actions that would happen in the first year to several years afterwards. Example: `polygon(yr, 0, 0, 1, 0, 1, 0.5, 10, 1)`; from year 0 to 1 the probability of management is 0. From year 1 to year 10 the management probability increases from 0.5 to 1.

-   

    ### `managementCapGrid` (filepath)

    Spatial grid (with user-defined resolution, e.g. 100x100km) that provides an upper limit to area managed (in hectares) per year and grid cell.

-   

    ### `managementCapModifier` (numeric)

    Multiplicative factor that modifies the value provided in `managementCapGrid.` For example, consider a 10x10km grid and a value in the grid of 1000, and `managementCapModifier` of 1.2. This would result in a maximum of forest management per year on the cell (100km2) of 1200 ha / year.

## Output

The module can provide tabular and gridded [output](outputs.md#Management).
