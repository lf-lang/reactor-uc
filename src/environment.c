#if defined(FEDERATED)
#include "./environments/environment_unfederated.c"
#include "./environments/environment_federated.c"
#else
#include "./environments/environment_unfederated.c"
#endif