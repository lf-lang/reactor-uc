package org.lflang.generator.uc

import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.inferredType
import org.lflang.lf.Parameter
import org.lflang.lf.Reactor
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

    // For the default POSIX platform we generate a main function, this is only for simplifing
    // quick testing. For real applications the code-generated sources must be included in an
    // existing project.
    fun generateMainFunction() = with(PrependOperator) {
        if (targetConfig.get(PlatformProperty.INSTANCE).platform == PlatformType.Platform.NATIVE) {
            """ 
            |// The following is to support convenient compilation of LF programs
            |// targeting POSIX. For programs targeting embedded platforms a 
            |// main function is not generated.
            |int main(int argc, char **argv) {
            |   lf_start();
            |}
        """.trimMargin()
        } else {
            ""
        }
        }

    fun getDuration() = if (targetConfig.isSet(TimeOutProperty.INSTANCE)) targetConfig.get(TimeOutProperty.INSTANCE).toCCode() else "FOREVER"

    fun keepAlive() = if(targetConfig.isSet(KeepaliveProperty.INSTANCE)) "true" else "false"

    fun fast() = if(targetConfig.isSet(FastProperty.INSTANCE)) "true" else "false"

    fun generateMainSource() = with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |LF_ENTRY_POINT(${main.codeType}, ${getDuration()}, ${keepAlive()}, ${fast()});
        ${" |"..generateMainFunction()}
        """.trimMargin()
    }

    fun generateMainHeader() = with(PrependOperator) {
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
}
