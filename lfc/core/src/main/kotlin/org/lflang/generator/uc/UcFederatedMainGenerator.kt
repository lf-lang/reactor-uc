package org.lflang.generator.uc

import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.target.property.FastProperty
import org.lflang.target.property.KeepaliveProperty
import org.lflang.target.property.TimeOutProperty

class UcFederatedMainGenerator(
    private val main: UcFederate,
    private val targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
) {

    private val top = main.inst.eContainer() as Reactor
    private val ucConnectionGenerator = UcConnectionGenerator(top, main)

    fun getDuration() = if (targetConfig.isSet(TimeOutProperty.INSTANCE)) targetConfig.get(TimeOutProperty.INSTANCE).toCCode() else "FOREVER"

    fun keepAlive() = if(targetConfig.isSet(KeepaliveProperty.INSTANCE)) "true" else "false"

    fun fast() = if(targetConfig.isSet(FastProperty.INSTANCE)) "true" else "false"

    fun generateStartSource() = with(PrependOperator) {
            """
            |#include "reactor-uc/reactor-uc.h"
            |#include "lf_federate.h"
            |static ${main.codeType} main_reactor;
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |void lf_start(void) {
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor);                                                               
            |    lf_environment.scheduler->duration = ${getDuration()};
            |    lf_environment.scheduler->keep_alive = ${keepAlive()};
            |    lf_environment.scheduler->leader = ${top.instantiations.first() == main.inst && main.bankIdx == 0};
            |    lf_environment.fast_mode = ${fast()};
            |    lf_environment.has_async_events  = ${main.inst.reactor.inputs.isNotEmpty()};
            |    ${main.codeType}_ctor(&main_reactor, NULL, &lf_environment);
            |    lf_environment.net_bundles_size = ${ucConnectionGenerator.getNumFederatedConnectionBundles()};
            |    lf_environment.net_bundles = (FederatedConnectionBundle **) &main_reactor._bundles;
            |    lf_environment.assemble(&lf_environment);
            |    lf_environment.start(&lf_environment);
            |    lf_exit();
            |}
        """.trimMargin()
    }

    fun generateStartHeader() = with(PrependOperator) {
        """
            |#ifndef REACTOR_UC_LF_MAIN_H
            |#define REACTOR_UC_LF_MAIN_H
            |
            |void lf_start(void);
            |
            |#endif
            |
        """.trimMargin()
    }

    fun generateMainSource() = with(PrependOperator) {
        """
            |#include "lf_start.h"
            |int main(void) {
            |  lf_start();
            |}
        """.trimMargin()
    }
}
