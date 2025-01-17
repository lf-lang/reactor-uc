
package org.lflang.generator.uc

import org.lflang.FileConfig
import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.getEventQueueSize
import org.lflang.generator.uc.UcReactorGenerator.Companion.getReactionQueueSize
import org.lflang.joinWithLn
import org.lflang.lf.Reactor
import org.lflang.target.property.BuildTypeProperty
import org.lflang.target.property.CmakeIncludeProperty
import org.lflang.target.property.PlatformProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.toUnixString
import org.lflang.unreachable
import org.lflang.util.FileUtil
import java.nio.file.Path
import java.time.LocalDateTime
import kotlin.io.path.name
import kotlin.math.max

class UcCmakeGenerator(private val main: Reactor, private val targetConfig: TargetConfig, private val fileConfig: FileConfig) {
    private val S = '$' // a little trick to escape the dollar sign with $S
    private val platform = targetConfig.get(PlatformProperty.INSTANCE).platform
    val includeFiles = targetConfig.get(CmakeIncludeProperty.INSTANCE)?.map { fileConfig.srcPath.resolve(it).toUnixString() }


    fun generateCmake(sources: List<Path>) =
        if (platform == PlatformType.Platform.NATIVE) {
            generateCmakePosix(sources)
        } else {
            generateCmakeEmbedded(sources)
        }

    fun generateCmakeEmbedded(sources: List<Path>) = with(PrependOperator) {
        """
            |# This file is generated by LFC. It is meant to be included in
            |# an existing CMake project. 
            |
            |set(LFC_GEN_SOURCES
        ${" |    "..sources.filterNot{it.name == "lf_main.c"}.joinWithLn { "$S{CMAKE_CURRENT_LIST_DIR}/${it.toUnixString()}"}}
            |)
            |set(LFC_GEN_MAIN "$S{CMAKE_CURRENT_LIST_DIR}/lf_main.c")
            |set(REACTOR_UC_PATH $S{CMAKE_CURRENT_LIST_DIR}/reactor-uc)
            |set(LFC_GEN_INCLUDE_DIRS $S{CMAKE_CURRENT_LIST_DIR})
            |set(REACTION_QUEUE_SIZE ${main.getReactionQueueSize()} CACHE STRING "Size of the reaction queue")
            |set(EVENT_QUEUE_SIZE ${main.getEventQueueSize()} CACHE STRING "Size of the event queue")
            |
        """.trimMargin()
    }

    fun generateCmakePosix(sources: List<Path>) = with(PrependOperator) {
        """
            |cmake_minimum_required(VERSION 3.10)
            |project(${fileConfig.name} VERSION 0.0.0 LANGUAGES C)
            |set(PLATFORM POSIX CACHE STRING "Target platform")
            |set(REACTION_QUEUE_SIZE ${max(main.getReactionQueueSize(), 1)} CACHE STRING "Size of the reaction queue")
            |set(EVENT_QUEUE_SIZE ${max(main.getEventQueueSize(), 1)} CACHE STRING "Size of the event queue")
            |set(CMAKE_BUILD_TYPE ${targetConfig.getOrDefault(BuildTypeProperty.INSTANCE)})
            |
            |set(LF_MAIN_TARGET ${fileConfig.name})
            |set(SOURCES
        ${" |    "..sources.joinWithLn { it.toUnixString() }}
            |)
            |add_executable($S{LF_MAIN_TARGET} $S{SOURCES})
            |install(TARGETS $S{LF_MAIN_TARGET}
            |        RUNTIME DESTINATION $S{CMAKE_INSTALL_BINDIR}
            |        OPTIONAL
            |)
            |
            |add_subdirectory(reactor-uc)
            |target_link_libraries($S{LF_MAIN_TARGET} PRIVATE reactor-uc)
            |target_include_directories($S{LF_MAIN_TARGET} PRIVATE $S{CMAKE_CURRENT_LIST_DIR})
            ${" |"..(includeFiles?.joinWithLn { "include(\"$it\")" } ?: "")}
        """.trimMargin()
    }
}
