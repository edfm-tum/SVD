## Setting up a project for SVD

An efficient way to set up the required spatial and tabular data is to use R as much as possible. For text files (such as the project configuration file), use a powerful editor such as Notepad++ that is capable of syntax highlighting. Using a spreadsheet program such as Excel is fine, but it is advisable to create the actual input files for SVD in R (e.g., by editing Excel, importing Excel into R, writing input files in R).

### Spatial data

SVD used one spatial grid that defines the extent of the landscape (see also [configuring the landscape](configuring_the_landscape.md)). This grid set with the `landscape.grid` key in the config file and is considered as the "master" grid. The grid defines:

-   the extent of the landscape (i.e., the "rectangle" around the actual project area)

-   the base resolution. SVD uses the resolution of this grid as the base resolution. 100m are the SVD default, but other resolutions can be used as well (note that some functions such as local neighborhood relationships depend inherently on the basic resolution)

-   the spatial projection used - the lower left corner of this specific grid is used as the origin (0/0) in SVD

-   all cells on the landscape that are part of the simulated area (use `NA` for cells not simulated).

Note that any cell with a valid value here is expected to have input also other data sources (e.g., initial state of cells, SVD throws errors otherwise). Similarly, other input are only allowed for valid cells in the landscape grid (e.g., providing a initial state for a cell that is `NA` will throw an error).

**Therefore**: Double-check all grids, particularly if you reproject / aggregate / combine data from different sources! Below are some examples how typical problems can be addressed.

```{r eval=FALSE}

lscp <- raster("....") # load raster
init.state <- raster("...") # load init state

extent(lscp) == extent(init.state) # check extent
crs(lscp) == crs(init.state) # check projection

# show pixels missing in the init layer
plot( is.na(init.state) & !is.na(lscp))

# show pixels missing in the landscape layer
plot( !is.na(init.state) & is.na(lscp))

# To fix problems, you can e.g. remove pixels (=set to NA) e.g. from the init layer
init.state[][ !is.na(init.state[]) & is.na(lscp[]) ] <- NA

### To change size / projection, use functions from the raster package such as
# crop(), extend(), resample() (with resample, be careful to use the right method, e.g. ngb for classes)



```

#### Environment

Variables that vary in space (think of site variables such as soil depth) are stored much like polygons in vector based GIS: space is divided into "polygons" and an attached data table specifies attributes for each polygon. For SVD, the spatial polygons are "rastered" to a grid with an `environmentId` as value. The attribute data is stored in table linking attributes to each `environmentId`. Note that the corner case of each cell being a "polygon" you can define attributes for each cell of the landscape.

Below is a simple setup with unique `environmentId`s for each cell on the landscape. Note that this setup may be inefficient for very large landscapes.

```{r eval=FALSE}

# assume we have a grid with all valid pixels, e.g. the init.state grid
lscp <- init.state.grid # make a copy

# generate a grid with a unique ID for each cell, starting with 1
# First, get the indices of all valid cells
lindex <- which(!is.na(init.state.grid[]))

# now overwrite all valid values with a sequence of 1 .. number of cells
lscp[lindex] <- 1:length(lindex)
plot(lscp) 

# assume further, we have a grid of some site attribute with matching
# extent and resolution. Say, we have matching layers for soil depth and for nutrients:

# in this case we can create a basic landscape table:
env.table <- data.frame(
     enviromentId = lscp[lindex],
     climateId = 1, # see below
     soilDepth = soil.depth[lindex],
     nutrients = nutriets[lindex]
)

write.csv(env.table, file="gis/landscape.txt", row.names=F, sep=".")

```

#### Initial state and residence time

SVD requires data for both the initial state and the residence time of each cell on the landscape. There are different mechanisms for loading this information, which are controlled with `initialState.mode` in the config file. If you specify `grid`, you need to provide grids for both. Note that you need to specify data for residence time even if you do not make of this variable. To generate a valid file, you can use the following code:

```{r eval=FALSE}

# assuming we have the initial state in the grid init.state:
# This sets 1 for all valid pixels (regardless of the current state)
init.restime <- init.state * 0 + 1 

# remember, set the datatype:
writeRaster(init.restime, "gis/init_restime.tif", datatype="INT4S", overwrite=T)

```

#### Climate data

Climate data - and more generally, all information that can change over time - uses tabular data with time series of the relevant data linked to via the `climateId` in the environment table. The most simple approach is to assume homogeneous climate for the whole landscape, and a table with dummy data. The climate data is homogeneous, when the same `climateId` is used for the whole landscape (see example code above, where `climateId` = 1 everywhere). To create a table with dummy data, you can:

```{r eval=FALSE}

# assume we have only cliateId = 1

# create a dummy time series for 1000 yrs 
dummy.climate <- data.frame( climateId = 1,
                             year = 1:1000) 
write.csv(dummy.climate, "climate/dummy_climate.csv", row.names = F)

```

Make sure to switch off the `climate.sequence` in the config file. Note, that in this setup no useful climate data is available - the data is necessary to make SVD happy.

A relatively simple way to provide spatial climate information and a change signal is as follows:

-   get gridded data for long-term averages and use the environment table to store a value for each cell

-   derive / assume a single time series for change in climatic parameters (e.g. a linear increase of temperature)

-   combine standard value with the change signal within SVD

Here is an example:

```{r eval=FALSE}

# see also the code snippet above: now we have climate data:
env.table <- data.frame(
     enviromentId = lscp[lindex],
     climateId = 1, # see below
     MAT = annual.temp[lindex],
     MAP = annual.precip[lindex]
)

# ....

cc.climate <- data.frame( climateId = 1,
                             year = 1:100,
                             dT = (1:100)/50, # linear increase up to 2 deg 
                             dP = 1 - (1:100)/500 )# decrease of precip by 20%
# ....

# now you can use *both* variables in expression 
# (make sure that climate.publishVariables is true)

# e.g. use a threshold of 800mm mean annual precip: if(MAP*dp < 800, 2, 1)

```

## Tips and Tricks

#### Tables

Be careful with the decimal separator for numbers ("." vs ","). Like most programming languages, SVD requires ".". To force correct decimal separators, use the `dec` parameter of `write.csv()` and related functions.

CSV files use a comma (",") as a separator. This can be a problem if complex expressions that contain commas are in your data. In that case, using a different separator (such as a tab character or white spaces) can be a good idea. Use the `sep` parameter of `write.csv()` and related functions. While at it, do not use quoting (`quote=F`), and avoid row names (`row.names=F`).

```{r eval=FALSE}
library(readxl)
# read data from Excel sheet. You can load from a specific sheet or range (see also the Import from Excel dialog)
states.tab <- read_xls("states.xlsx", sheet="states")

# process data

# write a CSV / TSV 
write.csv(states.tab, "stm/states.csv", row.names = F, quote=F, sep = "\t", dec=".")

```

#### Logging

You can control the level of details in the log file. It can be useful to increase the level of detail when hunting down problems. To do so, change the logging levels in the config file (e.g, `logging.setup.level` , see [project file](project_file.md)).

Use a capable editor that allows to reload the file whenever the file changes on disk (e.g., RStudio, Notepad++). Pro tip: use an editor that allows custom syntax highlighting (e.g. change appearance of warning/error lines).
