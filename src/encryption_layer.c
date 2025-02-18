#include "reactor-uc/encryption_layer.h"

#ifdef ENCRYPTION_LAYER_NO_ENCRYPTION
#include "./platform/posix/no_encryption.c"
#endif
