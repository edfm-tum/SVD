## Bark Beetle Module

**Purpose**

This module simulates the spread of bark beetle infestations within the landscape. It incorporates the effects of regional wind events, beetle susceptibility, climate, and probabilistic spread mechanics to realistically model bark beetle dynamics.

**Sequence of Steps**

-   Background infestation: Starts background infestations based on a probability function that may incorporate a regional background infestation probability grid. Susceptibility and climate data influence the chance of an infestation starting in a cell.

-   Wind-Beetle Interaction: Identifies cells that were affected by wind events in the previous year, and ncreases the susceptibility of these wind-disturbed cells, making them more likely to be infested by beetles.

-   Probabilistic Infestation Spread:

    -   Iteration: Processes active cells iteratively, representing one year of simulation per iteration.
    -   Impact: Infested cells undergo a state change (e.g., from live to dead) based on transition matrix provided in `transitionFile`.
    -   Dispersal: Beetles spread probabilistically from infested cells with their range determined by a climate-dependent kernel. Spread success in a target cell is influenced by its susceptibility and the probability value from the kernel.
    -   Mortality: Infested cells may experience mortality based on factors such as base mortality rate, frost events, and outbreak age.

**Details**

-   **Initial infestation:** for large landscapes, the model subsamples a different set of points to test in each year (for landscapes \> 10 Mio px the factor is 100). If `regionalBackgroundProb` is provided, the expression is used to calculate the probability for the cell. If also `regionalBackgroundProb` grid is provided, then the value of of the probability grid can be used in the expression as the variable `regionalProb`. A value of 0.000685 is used when no expression is given. The resulting probability is linearly scaled with the subsampling-factor.

-   **Climate Influences:** Climate data, particularly minimum temperatures and the number of frost days, affect beetle population growth and survival. This data is used to select the appropriate dispersal kernel and calculate mortality risk. Technically, the number of genereations for a given year, and the number of days with intense frost are input parameter and need to be provided with the climate data to SVD. The respective columns are configured with `climateVarGenerations` and `climateVarFrost`. See also `climate.firstAuxiliaryColumn`.

-   **Wind Interactions:** Wind events increase the likelihood of beetle infestations by creating more susceptible areas (e.g., through the creation of deadwood). The probability of starting a bark beetle outbreak on cells affected by wind in the previous simulation year is based on spruce proportion on the cell and a parameter, calculated as:

    $p_{outbreak} = prop_{spruce} \cdot windInteractionStrength$

-   **Probabilistic Spread:** The spread of beetles from infested cells is not deterministic. The probability of a successful spread depends on the susceptibility of the target cell as well as the dispersal probabilities encoded in the kernel. The kernel is pre-processed and loaded into SVD via `kernelFile`.

-   **Bark beetle kernels:** Are created in R using a simple "simulation" approach:

    -   We simulate n beetles (or beetle units). Each beetle starts from a random position on a 100x100 cell (here n = 10000)
    -   Each beetle travels a distance sampled from beetle distribution kernel from Kautz (as in iLand) and in random direction
    -   If there are more than 1 generations, then for every landed beetle (unit), k offspring beetles are created that again travel in random direction (and with distance sampled from the kernel).
    -   Repeat for number of generations

    This process yields "kernels", i.e. what density of beetles travel to neighboring (100m) cells for different generation counts. Kernels have a size of 121x121, with the beetle source in the center, thus spanning 1km in each direction, see below for definition of the file format.

-   **Bark beetle generations:** The number of bark beetle generations in a single year is driven by beetle phenology, and thus strongly by climate. We use the "PhenIPS" approach (also used in iLand) and calculate the potential number of beetle generations based on daily climate (t_min, t_max, radiation), an assumption on LAI, and latitude. We use a C++ implementation (based on iLand's) embedded in R to pre-process climate data.

**Configuration**

The module is configured in the project file. In addition to the `enabled` and `type` setting, the bark beetle module offers these options:

-   

    ### `climateVarGenerations` (string)

    Name of the climate variable with the number of potential bark beetle generations (`Pheinps`).

-   

    ### `climateVarFrost` (string)

    Name of the climate variable with the number of days where the minimum temperature falls below -15 °C.

-   

    ### `beetleOffspringFactor` (integer)

    Factor impacting the size of the dispersal kernel. Use a value defined in the kernel file `kernelFile`(e.g., 1, 1.5, 2, 2.5, 3).

-   

    ### `kernelFile` (filepath)

    Filepath to a CSV file containing the dispersal kernel data (see table below for format).

-   

    ### `successOfColonization` (double)

    Global scaling factor for colonization success, influencing the overall spread rate.

-   

    ### `backgroundProbFormula` (string)

    (Optional) Formula to calculate background infestation probability (see expression syntax). Note that you can include the value of `regionalBackgroundProb` as well as climate variables in the expression Uses a variable `regionalProb`. If blank the

-   

    ### `regionalBackgroundProb` (filepath)

    (Optional) Filepath to a grid with regional background infestation probabilities.

-   

    ### `windInteractionStrength` (double)

    Factor determining the increase in susceptibility of wind-disturbed cells to beetle infestations.

-   

    ### `transitionFile` (filepath)

    Filepath to the transition matrix file defining the allowed state changes after beetle infestation.

-   

    ### `stateFile` (filepath)

    Filepath to the file defining state-specific properties (like bark beetle susceptibility)

-   

    ### `saveDebugGrids` (boolean)

    Enables saving of intermediate debugging grids.

**Input Data**

**kernelFile**

This file defines the bark beetle dispersal kernel. It has a CSV format with the following columns:

| Column | Description                                                                                                         | Data Type |
|------------|-------------------------------------------------|------------|
| vgen   | Number of potential bark beetle generations per year (should match values used for `beetleOffspringFactor`)         | double    |
| vk     | Offspring factor (should match the configured value in `beetleOffspringFactor`)                                     | integer   |
| ...    | 121 further columns, representing dispersal probabilities to cells in a grid of 11x11 cells around the source cell. | double    |

**State-Specific Variables**

The bark beetle module requires the following state-specific variables. These are defined in the file specified by the `stateFile` option.

| Variable Name     | Description                                                                     | Data Type |
|----------------|-----------------------------------------|----------------|
| pBarkBeetleDamage | Susceptibility of the state to bark beetle infestation (value between 0 and 1). | double    |

**Output**

The module can provide tabular and gridded [output](outputs.md#BarkBeetle).
