# Overview

The beginning of each source file (`.c`) contains a short description about rationale used for key implementation features (e.g. use of synchronisation variables, etc.). Header files (`.h`) only contain method prototypes implemented in their respective source files.

`runtest.c` contains all the tests developed to debug the program implemented. Individual methods call `gen_blank_files()` to reset the three main filesystem files opened/created in `main()`.

Two scripts are included for generating coverage statistics, `gcov.sh` and `lcov.sh`. Running these scripts will create directory `cov`, copy source and header files to the directory, and either output coverage data or generate HTML coverage reports, respectively. A separate directory, `cov` is used for these commands as they circumvent issues that were encountered during development. Besides the HTML coverage reports, the content of  the `cov` folder can otherwise be ignored as they are duplicates of files in the root Ed workspace directory.

**IMPORTANT:** To run the scripts on Ed, `chmod +x gcov.sh lcov.sh` should be run first.

**Note:** Since Ed does not support `lcov`, interactive HTML coverage reports generated locally prior to submission are available in `cov/cov-html/index.html`.