package org.lflang.generator.uc

import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.inferredType
import org.lflang.lf.Instantiation
import org.lflang.lf.Parameter
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.target.property.FastProperty
import org.lflang.target.property.KeepaliveProperty
import org.lflang.target.property.PlatformProperty
import org.lflang.target.property.TimeOutProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.toUnixString

class UcMainGenerator(
    private val main: Reactor,
    private val targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
) {

    private val ucParameterGenerator = UcParameterGenerator(main)
    private val ucConnectionGenerator = UcConnectionGenerator(main, null, emptyList())

    fun getDuration() = if (targetConfig.isSet(TimeOutProperty.INSTANCE)) targetConfig.get(TimeOutProperty.INSTANCE).toCCode() else "FOREVER"

    fun keepAlive() = if(targetConfig.isSet(KeepaliveProperty.INSTANCE)) "true" else "false"

    fun fast() = if(targetConfig.isSet(FastProperty.INSTANCE)) "true" else "false"

    fun generateStartSource() = with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |static ${main.codeType} main_reactor;
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |void lf_start(void) {
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor);                                                               
            |    ${main.codeType}_ctor(&main_reactor, NULL, &lf_environment ${ucParameterGenerator.generateReactorCtorDefaultArguments()});
            |    lf_environment.scheduler->duration = ${getDuration()};
            |    lf_environment.scheduler->keep_alive = ${keepAlive()};
            |    lf_environment.fast_mode = ${fast()};
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
