
// this file includes the correct hardware/platform implementation

#ifdef POSIX
#include "hardware/posix/posix_implementation.c"
#endif // POSIX

#ifdef RIOT
#include "hardware/riot/riot_implementation.c"
#endif // RIOT
