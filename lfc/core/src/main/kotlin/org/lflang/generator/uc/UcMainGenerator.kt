package org.lflang.generator.uc

import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.inferredType
import org.lflang.lf.Parameter
import org.lflang.lf.Reactor
import org.lflang.target.property.PlatformProperty
import org.lflang.target.property.TimeOutProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.toUnixString

class UcMainGenerator(
    private val main: Reactor,
    private val targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
) {

    // For the default POSIX platform we generate a main function, this is only for simplifing
    // quick testing. For real applications the code-generated sources must be included in an
    // existing project.
    fun generateMainFunction() = with(PrependOperator) {
        if (targetConfig.get(PlatformProperty.INSTANCE).platform == PlatformType.Platform.AUTO) {
            """ 
            |// The following is to support convenient compilation of LF programs
            |// targeting POSIX. For programs targeting embedded platforms a 
            |// main function is not generated.
            |int main(int argc, char **argv) {
            |   lf_main();
            |}
        """.trimMargin()
        } else {
            ""
        }
        }

    fun generateMainSource() = with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |static Environment env;
            |static ${main.codeType} main_reactor;
            |void lf_main(void) {
            |   Environment_ctor(&env, &main_reactor.super);
            |   ${if (targetConfig.isSet(TimeOutProperty.INSTANCE)) "env.scheduler.set_duration(&env.scheduler, ${targetConfig.get(TimeOutProperty.INSTANCE).toCCode()});" else ""}
            |   ${main.codeType}_ctor(&main_reactor, &env, NULL);
            |   env.assemble(&env);
            |   env.start(&env);
            |}
        ${" |"..generateMainFunction()}
        """.trimMargin()
    }

    fun generateMainHeader() = with(PrependOperator) {
        """
            |#ifndef REACTOR_UC_LF_MAIN_H
            |#define REACTOR_UC_LF_MAIN_H
            |
            |void lf_main(void);
            |
            |#endif
            |
        """.trimMargin()
    }
}
