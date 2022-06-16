# Moola - Multicore-Cache-Simulator

Moola is a multicore cache simulator that is similar to Dinero but permits traces from multiple cores and threads. Moola can be easily configured to suit different cache memory research.

# Related Publications.

Charles Shelor and Krishna Kavi. ["Moola: Multicore Cache Simulator"](https://csrl.cse.unt.edu/kavi/Research/CATA-2015.pdf), 30th International Conference on Computers and Their Applications (CATA-2015), March 9-11, 2015, Honolulu, Hawaii.

Mosquera, Fernando, et al. ["CHASM: Security Evaluation of Cache Mapping Schemes."](https://link.springer.com/chapter/10.1007/978-3-030-60939-9_17), International Conference on Embedded Computer Systems. Springer, Cham, 2020.

# How to compile

You have two moola version

1) The original moola is inside the **moola_src** folder. To compile it, you have to run Makefile

2) The modified version of moola is inside the **modified_moola_src** folder. This version add the information leakage and new mapping set schemes. To compile it, run Makefile

# List of Schemes in modified moola

This is the final list of schemes for moola mod

0 --> Standard Module.

1 --> Rotate by 3.

2 --> XOR.

3 --> Rotate right by 1 and XOR.

4 --> Square TAG.

5 --> Odd multiplier by 7.


6 --> Intel slide Cache.

7 --> DES (Similar to CAESAR).

8 --> CAESAR.
