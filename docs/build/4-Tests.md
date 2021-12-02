# Tests

Tests compilation  can be enabled at configure time using the option `-DBUILD_TESTING=ON` (`OFF` by default)

After build, tests can be run with ``ctest`` :

```
cd _build
ctest -C Release --output-on-failure
```

All tests are associated to a label and multiple labels can be defined. You can choose which tests will be executed at ctest run.

This is the list of the available labels :

| Name     | Label |Description |
|:-------|-----|-----|
| `unit_tests`  | `unit`  | Unit test for OR-Tools use and lpnamer|
| `unit_logger`  | `unit`  | Unit test for logger use|
| `unit_launcher`  | `unit`  |Unit test Antares-Xpansion python launcher|
| `examples_medium_sequential`  | `medium` `medium_sequential` |Non-parallel end-to-end tests with examples antares studies (medium duration)|
| `examples_medium_mpi`  | `medium` `medium_mpi` |Parallel end-to-end tests with examples antares study (medium duration)|
| `examples_long`  | `long`  |End to end tests with examples antares study (long duration)|
| `mpibenders`  | `benders-mpi`  |End to end tests for benders mpi optimization|
| `sequential`  | `sequential`  |End to end tests for benders sequential optimization|
| `merge_mps`  | `merge-mps`  |End to end tests for merge mps optimization|

Note :
> Use `ctest -N` to see all available tests

Here is an example for running only examples_medium tests (use of `Name` with `-R` option):

```
ctest -C Release --output-on-failure -R examples_medium
```

Here is an example for running only units tests (use of `Label` with `-L` option):

```
ctest -C Release --output-on-failure -L unit
```

To run all test, don't indicate any label or name:

```
ctest -C Release --output-on-failure
```