# Cache Coherence Simulator

This code repository is the implementation of a cache coherence simulator in partial fulfilment of the final project of CSE240B at University of California, San Diego. This simulator supports 4 different coherence protocols - MSI, MESI, MOSI and MOESI. The modelling of the cache is done in C++. The testing is done using the two traces of canneal as well as 2 microbenchmarks which we have written ourselves.

Protocol_testcase file contains our analysis of the coherence protocols. We outline all the possible fields and evaluate which fields are valid. We then check whether the particular test case is covered in our model.
