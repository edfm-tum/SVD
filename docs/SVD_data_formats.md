# Data formats

-   [Configuration files](#config_file)
-   [Data tables](#table)
-   [Raster data](#raster)

## <a name="config_file"></a> Configuration files 
SVD uses a simple text file based format for the configuration of the model. The model reads single configuration items as key-value pairs from the file.

The general layout is:

`key = <value>`

The `key` is a string that may contain `.` characters for logical grouping. The `value` is a string which is interpreted by the model. Possible data types are `string`, `numeric`, and `bool` (with the possible values `true` and `false`).

-   lines that start with a `#` are considered as comments, empty lines are ignored
-   The sequence of items is not important
-   Missing keys are (in general) not allowed and produce errors

Example:

    output.StateGrid.enabled = true
    output.StateGrid.path = output/state_$year$.asc
    output.StateGrid.interval = 20

This specifies that the output (`output.`) StateGrid (`output.StateGrid`) is `enabled` (boolean value), output data is saved at a specific location (`path`, a string), and with a given `interval` (numeric).

## <a name="table"></a> Data tables 
Tabular data is read into SVD as delimited text tables.

-   Comments: lines starting with `#` are considered as comment and ignored
-   the first (non-comment) line has the column names (quotes (`"`) are removed)
-   the delimiter is auto-detected and can be one of: tab (`\t`), semicolon (`;`), colon (`,`), or space ()
-   floating point values must use `.` as decimal separator and can use scientific notation (e.g. 0.1234 or 1.23e02)
-   the file extension is not considered

Example:

    "id","climateId","availableNitrogen","pctSand"
    1,4368,47,50.9185174306234
    2,4368,48,50.9185174306234
    3,4368,49,50.9185174306234
    ....

## <a name="raster"></a> Raster data 

SVD supports GeoTIFF and simple [ASCII file format](https://en.wikipedia.org/wiki/Esri_grid) for spatial data. Such grids can be produced by GIS Software (e.g., QGis, Arcmap) or also by the R `raster` or `terra` packages.

The coordinates system has to be metric, and the requirements for the cell size vary (a typical cell size is 100m). SVD uses internally a flat metric coordinate system without considering spatial projections. Therefore all spatial input data should use a consistent projection.

SVD uses the [FreeImage](https://freeimage.sourceforge.io/) library for handling GeoTIFF files. Currently, the selection of data types is limited to INT16 (16-bit signed integers) and INT32 (32-bit signed integer) for integer grids, and 32 bit / 64 bit floating point data. In R you can use the `datatype` parameter of `writeGrid()` to force a specific format (use 'INT2S' or 'INT4S' for 16 and 32 bit integers, and 'FLT4S' for 32 bit float or 'FLT8S' for 64 bit double precision floating point numbers).

The origin (i.e. the 0/0 point of the internal coordinate system) is derived from the [`landscape.grid`](configuring_the_landscape.md).
