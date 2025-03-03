# SVDc

The SVD console application ("*SVDc*") is a command-line version of SVD. It contains the same core functionality as the version with a user interface ([SVD UI](svdUI.md)) (actually, both applications use the same code base), but does not use a graphical user interface. The main purpose of SVDc is for automating simulation jobs and/or for running in environments where no graphical user interface is available. The name of the executable of the console version is `SVDc` (whereas the name of the GUI version is `SVD`).

## Instructions

Run SVDc from a shell with options:

```         
SVDc config-file years additional-options
```

-   `config-file` path of a valid SVD configuration file.

-   `years`: the number of years to simulate

-   `additonal-options`: additional key=value pairs that represent values for settings that are set after the project is loaded, i.e., to change values given in the config file. If the value contains and space characters, the value string must be enclosed by "-characters.

A typical use is to run multiple simulations, and use different input data for each simulation (think climate data) and store the results to a specific location. Here is an example:

``` bash
# run this script 

svdc E:\Daten\SVD\projects\gye\config_historic.conf 142 filemask.run=41 modules.fire.ignitionFile=fire/ignitions/HadGEM2-ES-85_1.txt climate.file=climate/historic.txt 
svdc E:\Daten\SVD\projects\gye\config_historic.conf 142 filemask.run=42 modules.fire.ignitionFile=fire/ignitions/HadGEM2-ES-85_2.txt climate.file=climate/historic.txt
```

This script:

-   loads the project `config_historic.conf`. All relative file paths are resolved relative to the `gye` folder where the config file is located

-   The simulation duration is set to 142 years

-   the `filemask.run` setting is set - this can be used to change *multiple* output file paths

-   similarly, a specific `ignitionFile` for the fire module, and a `climate.file` is set. Note that relative paths are always resolved relative to the location of the config file.
