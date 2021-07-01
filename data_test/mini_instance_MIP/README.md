# Data test mini_instance_MIP

## Brief

This test case is a toy MIP problem to test the resolution of a stochastic problem by AntaresXpansion optimization modules :

* benderssequential
* bendersmpi
* merge_mps

This data is used in the test bendersEndToEnd.

## Problem formulation

min 1.5 x + z1 + 2 z2

sc:

    0 <= x <= 10

    x + z1 >= 1.5

    x + z2 >= 2.5

    0 <= z1

    0 <= z2

    x integer
    