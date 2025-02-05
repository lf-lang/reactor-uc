#include "reactor-uc/macros.h"
#include "Connections.h"
#include "reactor-uc/schedulers/static/scheduler_instructions.h"

const inst_t **static_schedule = NULL;

LF_ENTRY_POINT(Reactor_Connections, NEVER, false, false);

int main() {
    lf_start();
}