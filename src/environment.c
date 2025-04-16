#include "./environments/base_environment.c"

#if defined(FEDERATED)
#include "./environments/federate_environment.c"
#endif 

#if defined(ENCLAVED)
#include "./environments/enclave_environment.c"
#endif

