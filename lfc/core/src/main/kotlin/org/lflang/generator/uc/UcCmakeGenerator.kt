
package org.lflang.generator.uc

import org.lflang.FileConfig
import org.lflang.target.TargetConfig
import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcReactorGenerator.Companion.getEventQueueSize
import org.lflang.generator.uc.UcReactorGenerator.Companion.getReactionQueueSize
import org.lflang.joinWithLn
import org.lflang.lf.Reactor
import org.lflang.target.property.BuildTypeProperty
import org.lflang.target.property.PlatformProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.toUnixString
import org.lflang.unreachable
import org.lflang.util.FileUtil
import java.nio.file.Path
import java.time.LocalDateTime

class UcCmakeGenerator(private val main: Reactor, private val targetConfig: TargetConfig, private val fileConfig: FileConfig) {
    private val S = '$' // a little trick to escape the dollar sign with $S
    private val platform = targetConfig.get(PlatformProperty.INSTANCE).platform
    private val platformName = if (platform == PlatformType.Platform.AUTO) "POSIX" else platform.name.uppercase()

    fun generateCmake(sources: List<Path>) =
        if (platform == PlatformType.Platform.AUTO) {
            generateCmakePosix(sources)
        } else {
            generateCmakeEmbedded(sources)
        }

    fun generateCmakeEmbedded(sources: List<Path>) = with(PrependOperator) {
        """
            |# This is a generated CMakeLists file for ${fileConfig.name} at ${LocalDateTime.now()}
            |
            |set(LF_SOURCES_RELATIVE
        ${" |    "..sources.joinWithLn { it.toUnixString() }}
            |)
            |set(LF_SOURCES_ABSOLUTE
        ${" |    "..sources.joinWithLn { "$S{CMAKE_CURRENT_LIST_DIR}/${it.toUnixString()}"}}
            |)
            |set(REACTOR_UC_PATH $S{CMAKE_CURRENT_LIST_DIR}/reactor-uc)
            |set(LF_INCLUDE_DIRS $S{CMAKE_CURRENT_LIST_DIR})
            |set(REACTION_QUEUE_SIZE ${main.getReactionQueueSize()} CACHE STRING "Size of the reaction queue")
            |set(EVENT_QUEUE_SIZE ${main.getEventQueueSize()} CACHE STRING "Size of the event queue")
            |
        """.trimMargin()
    }

    fun generateCmakePosix(sources: List<Path>) = with(PrependOperator) {
        """
            |cmake_minimum_required(VERSION 3.5)
            |project(${fileConfig.name} VERSION 0.0.0 LANGUAGES C)
            |set(PLATFORM $platformName CACHE STRING "Target platform")
            |set(REACTION_QUEUE_SIZE ${main.getReactionQueueSize()} CACHE STRING "Size of the reaction queue")
            |set(EVENT_QUEUE_SIZE ${main.getEventQueueSize()} CACHE STRING "Size of the event queue")
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
            |target_link_libraries($S{LF_MAIN_TARGET} PUBLIC reactor-uc)
            |
        """.trimMargin()
    }
}
