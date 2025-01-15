
package org.lflang.generator.uc

import org.lflang.*
import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcInstanceGenerator.Companion.isAFederate
import org.lflang.lf.Instantiation
import org.lflang.lf.Reactor
import org.lflang.target.property.BuildTypeProperty
import org.lflang.target.property.CmakeIncludeProperty
import org.lflang.target.property.LoggingProperty
import org.lflang.target.property.PlatformProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil
import java.nio.file.Path
import java.time.LocalDateTime
import kotlin.io.path.name
import kotlin.math.max

abstract class UcCmakeGenerator(private val targetConfig: TargetConfig, private val fileConfig: UcFileConfig, private val numEvents: Int, private val numReactions: Int) {
    protected val S = '$' // a little trick to escape the dollar sign with $S
    private val minCmakeVersion = "3.10"
    protected val includeFiles = targetConfig.get(CmakeIncludeProperty.INSTANCE)?.map { fileConfig.srcPath.resolve(it).toUnixString() }
    protected val platform = targetConfig.get(PlatformProperty.INSTANCE).platform
    abstract val mainTarget: String
    abstract fun generateCmake(sources: List<Path>): String

    protected fun generateCmakeCommon(sources: List<Path>, compileDefs: List<String>) = with(PrependOperator) {
        """ |
            |set(LFC_GEN_SOURCES
        ${" |    "..sources.filterNot{it.name == "lf_main.c"}.joinWithLn { "$S{CMAKE_CURRENT_LIST_DIR}/${it.toUnixString()}"}}
            |)
            |set(LFC_GEN_COMPILE_DEFS
        ${" |    "..compileDefs.joinWithLn { it }}
            |)
            |set(LFC_GEN_MAIN "$S{CMAKE_CURRENT_LIST_DIR}/lf_main.c")
            |set(REACTOR_UC_PATH $S{CMAKE_CURRENT_LIST_DIR}/reactor-uc)
            |set(LFC_GEN_INCLUDE_DIRS $S{CMAKE_CURRENT_LIST_DIR})
            |set(REACTION_QUEUE_SIZE ${max(numReactions, 1)} CACHE STRING "Size of the reaction queue")
            |set(EVENT_QUEUE_SIZE ${max(numEvents, 1)} CACHE STRING "Size of the event queue")
        """.trimMargin()
    }

    protected fun generateCmakePosix(sources: List<Path>, compileDefs: List<String>) = with(PrependOperator) {
        """
            |cmake_minimum_required(VERSION $minCmakeVersion)
            |project(${mainTarget} LANGUAGES C)
            |set(LF_MAIN_TARGET ${mainTarget})
            |set(CMAKE_BUILD_TYPE ${targetConfig.getOrDefault(BuildTypeProperty.INSTANCE)})
            |set(PLATFORM POSIX CACHE STRING "Target platform")
        ${" |"..generateCmakeCommon(sources, compileDefs)}
            |add_executable($S{LF_MAIN_TARGET} $S{LFC_GEN_SOURCES} $S{LFC_GEN_MAIN})
            |install(TARGETS $S{LF_MAIN_TARGET}
            |        RUNTIME DESTINATION $S{CMAKE_INSTALL_BINDIR}
            |        OPTIONAL
            |)
            |add_compile_definitions("LF_LOG_LEVEL_ALL=LF_LOG_LEVEL_${targetConfig.getOrDefault(LoggingProperty.INSTANCE).name.uppercase()}")
            |add_compile_definitions($S{LFC_GEN_COMPILE_DEFS})
            |add_subdirectory($S{REACTOR_UC_PATH})
            |target_link_libraries($S{LF_MAIN_TARGET} PRIVATE reactor-uc)
            |target_include_directories($S{LF_MAIN_TARGET} PRIVATE $S{LFC_GEN_INCLUDE_DIRS})
        ${" |"..(includeFiles?.joinWithLn { "include(\"$it\")" } ?: "")}
        """.trimMargin()
    }
}

class UcCmakeGeneratorNonFederated(private val mainDef: Instantiation, targetConfig: TargetConfig, fileConfig: UcFileConfig, numEvents: Int, numReactions: Int): UcCmakeGenerator(targetConfig, fileConfig, numEvents, numReactions) {
    override val mainTarget = fileConfig.name

    override fun generateCmake(sources: List<Path>) =
        if (platform == PlatformType.Platform.NATIVE) {
            generateCmakePosix(sources, emptyList())
        } else {
            generateCmakeCommon(sources, emptyList())
        }
}

class UcCmakeGeneratorFederated(private val federate: UcFederate, targetConfig: TargetConfig, fileConfig: UcFileConfig, numEvents: Int, numReactions: Int): UcCmakeGenerator(targetConfig, fileConfig, numEvents, numReactions) {
    override val mainTarget = federate.codeType

    override fun generateCmake(sources: List<Path>) =
        if (platform == PlatformType.Platform.NATIVE) {
            generateCmakePosix(sources, federate.getCompileDefs())
        } else {
            generateCmakeCommon(sources, federate.getCompileDefs())
        }
}
