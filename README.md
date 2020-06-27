# Virtual File System

The primary file containing the filesystem implementation is `myfilesystem.c`.

The beginning of each source file (`.c`) contains a short description about rationale used for key implementation features (e.g. use of synchronisation variables, etc.). Header files (`.h`) only contain method prototypes implemented in their respective source files.

`runtest.c` contains all the tests developed to debug the program implemented. Individual methods call `gen_blank_files()` to reset the three main filesystem files opened/created in `main()`.

Two scripts are included for generating coverage statistics, `gcov.sh` and `lcov.sh`. Running these scripts will create directory `cov`, copy source and header files to the directory, and either output coverage data or generate HTML coverage reports, respectively.
