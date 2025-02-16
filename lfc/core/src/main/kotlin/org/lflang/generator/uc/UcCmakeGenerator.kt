package org.lflang.generator.uc

import java.nio.file.Path
import kotlin.io.path.name
import org.lflang.*
import org.lflang.generator.PrependOperator
import org.lflang.lf.Instantiation
import org.lflang.target.TargetConfig
import org.lflang.target.property.*

abstract class UcCmakeGenerator(
    private val targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
) {
  protected val S = '$' // a little trick to escape the dollar sign with $S
  private val minCmakeVersion = "3.10"
  protected val includeFiles =
      targetConfig.get(CmakeIncludeProperty.INSTANCE)?.map {
        fileConfig.srcPath.resolve(it).toUnixString()
      }
  protected val platform = targetConfig.get(PlatformProperty.INSTANCE).platform
  abstract val mainTarget: String

  abstract fun generateIncludeCmake(sources: List<Path>): String

  fun doGenerateIncludeCmake(sources: List<Path>, compileDefs: List<String>) =
      with(PrependOperator) {
        """ |# This file was generated by the Lingua Franca Compiler for the program $mainTarget
            |# It should be included by a CMakeLists.txt to build the final binary.
            |
            |set(LFC_GEN_SOURCES
        ${" |    "..sources.filterNot{it.name == "lf_main.c"}.joinWithLn { "$S{CMAKE_CURRENT_LIST_DIR}/${it.toUnixString()}"}}
            |)
            |set(LFC_GEN_COMPILE_DEFS
        ${" |    "..compileDefs.joinWithLn { it }}
            |)
            |set(LFC_GEN_MAIN "$S{CMAKE_CURRENT_LIST_DIR}/lf_main.c")
            |set(REACTOR_UC_PATH $S{CMAKE_CURRENT_LIST_DIR}/reactor-uc)
            |set(LFC_GEN_INCLUDE_DIRS $S{CMAKE_CURRENT_LIST_DIR})
        """
            .trimMargin()
      }

  fun generateMainCmakeNative() =
      with(PrependOperator) {
        """
            |# This file was generated by the Lingua Franca Compiler for the program $mainTarget.
            |# It is a standalone CMakeLists.txt intended for compiling the program for native execution.
            |
            |cmake_minimum_required(VERSION $minCmakeVersion)
            |project(${mainTarget} LANGUAGES C)
            |set(LF_MAIN_TARGET ${mainTarget})
            |set(CMAKE_BUILD_TYPE ${targetConfig.getOrDefault(BuildTypeProperty.INSTANCE)})
            |set(PLATFORM POSIX CACHE STRING "Target platform")
            |include($S{CMAKE_CURRENT_SOURCE_DIR}/src-gen/$S{LF_MAIN}/Include.cmake)
            |#include(./Include.cmake)
            |#add_executable($S{LF_MAIN_TARGET} $S{LFC_GEN_SOURCES} $S{LFC_GEN_MAIN})
            |#install(TARGETS $S{LF_MAIN_TARGET}
            |#        RUNTIME DESTINATION $S{CMAKE_INSTALL_BINDIR}
            |#        OPTIONAL
            |#)
            |#add_compile_definitions("LF_LOG_LEVEL_ALL=LF_LOG_LEVEL_${targetConfig.getOrDefault(LoggingProperty.INSTANCE).name.uppercase()}")
            |#add_compile_definitions($S{LFC_GEN_COMPILE_DEFS})
            |#add_subdirectory($S{REACTOR_UC_PATH})
            |#target_link_libraries($S{LF_MAIN_TARGET} PRIVATE reactor-uc)
            |#target_include_directories($S{LF_MAIN_TARGET} PRIVATE $S{LFC_GEN_INCLUDE_DIRS})
        ${" |"..(includeFiles?.joinWithLn { "include(\"$it\")" } ?: "")}
        """
            .trimMargin()
      }
}

class UcCmakeGeneratorNonFederated(
    private val mainDef: Instantiation,
    targetConfig: TargetConfig,
    fileConfig: UcFileConfig,
) : UcCmakeGenerator(targetConfig, fileConfig) {
  override val mainTarget = fileConfig.name

  override fun generateIncludeCmake(sources: List<Path>) =
      doGenerateIncludeCmake(sources, emptyList())
}

class UcCmakeGeneratorFederated(
    private val federate: UcFederate,
    private val targetConfig: TargetConfig,
    fileConfig: UcFileConfig,
) : UcCmakeGenerator(targetConfig, fileConfig) {
  override val mainTarget = federate.codeType

  override fun generateIncludeCmake(sources: List<Path>) =
      doGenerateIncludeCmake(sources, federate.getCompileDefs())
}
