package org.lflang.generator.uc

import org.lflang.MessageReporter
import org.lflang.lf.Instantiation
import org.lflang.reactor
import org.lflang.target.TargetConfig
import org.lflang.target.property.NoCompileProperty
import org.lflang.target.property.PlatformProperty
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil
import java.nio.file.Files
import java.nio.file.Path

class UcFederatedTemplateGenerator(private val mainDef: Instantiation, private val federate: UcFederate, private val targetConfig: TargetConfig, private val projectsRoot: Path, private val messageReporter: MessageReporter) {

    private val targetName: String = federate.codeType
    private val projectRoot = projectsRoot.resolve(federate.name)
    private val S = '$' // a little trick to escape the dollar sign with $S


    private fun generateCmake(init: String, mainTargetName: String, createMainTarget: Boolean) =
        """
            |cmake_minimum_required(VERSION 3.20.0)
            |$init
            |set(LF_MAIN ${mainDef.name})
            |set(LF_MAIN_TARGET $mainTargetName)
            |set(PROJECT_ROOT ${projectsRoot}/..)
            |set(FEDERATE ${federate.name})
            |project(${mainDef.name}_${targetName})
            |
            |${if (createMainTarget) "add_executable($S{LF_MAIN_TARGET})" else ""}
            |include(${S}ENV{REACTOR_UC_PATH}/cmake/lfc.cmake)
            |lf_setup()
            |lf_run_lfc($S{PROJECT_ROOT}/src $S{LF_MAIN})
            |lf_build_generated_code($S{LF_MAIN_TARGET} $S{CMAKE_CURRENT_SOURCE_DIR}/src-gen/$S{LF_MAIN}/$S{FEDERATE})
            |
            """.trimMargin()


    private fun generateFilesZephyr() {
        val cmake = generateCmake(init = """
            |set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")
            |set(BOARD "native_sim")
            |find_package(Zephyr REQUIRED HINTS ${S}ENV{ZEPHYR_BASE})
        """.trimIndent(),
            mainTargetName = "app",
            createMainTarget = false)
        FileUtil.writeToFile(cmake, projectRoot.resolve("CMakeLists.txt"))

        val prjConf = """
            |CONFIG_ETH_NATIVE_POSIX=n
            |CONFIG_NET_DRIVERS=y
            |CONFIG_NETWORKING=y
            |CONFIG_NET_TCP=y
            |CONFIG_NET_IPV4=y
            |CONFIG_NET_SOCKETS=y
            |CONFIG_POSIX_API=y
            |CONFIG_MAIN_STACK_SIZE=16384
            |CONFIG_HEAP_MEM_POOL_SIZE=1024
            |
            |# Network address config
            |CONFIG_NET_CONFIG_SETTINGS=y
            |CONFIG_NET_CONFIG_NEED_IPV4=y
            |CONFIG_NET_CONFIG_MY_IPV4_ADDR="127.0.0.1"
            |CONFIG_NET_SOCKETS_OFFLOAD=y
            |CONFIG_NET_NATIVE_OFFLOADED_SOCKETS=y
        """.trimMargin()
        FileUtil.writeToFile(prjConf, projectRoot.resolve("prj.conf"))
    }

    private fun generateFilesRiot() {
        val make = """
            |LF_MAIN ?= ${mainDef.name}
            |LF_FED ?= ${federate.name}
            |
            |# Execute the LF compiler if build target is "all"
            |ifeq ($S(firstword $S(MAKECMDGOALS)),all)
            | _ :=  $S(shell $S(REACTOR_UC_PATH)/lfc/bin/lfc-dev $S(CURDIR)/../../src/$S(LF_MAIN).lf -n -o $S(CURDIR))
            |endif
            |
            |# ---- RIOT specific configuration ----
            |# This has to be the absolute path to the RIOT base directory:
            |RIOTBASE ?= $S(CURDIR)/RIOT
            |
            |# If no BOARD is found in the environment, use this default:
            |BOARD ?= native
            |
            |# Comment this out to disable code in RIOT that does safety checking
            |# which is not needed in a production environment but helps in the
            |# development process:
            |DEVELHELP ?= 1
            |
            |# Change this to 0 show compiler invocation lines by default:
            |QUIET ?= 1
            |
            |include $S(REACTOR_UC_PATH)/make/riot/riot-lfc.mk
        """.trimMargin()
        FileUtil.writeToFile(make, projectRoot.resolve("Makefile"))

    }

    private fun generateFilesPico() {

    }

    private fun generateFilesNative() {
        val cmake = generateCmake("", federate.name, true)
        FileUtil.writeToFile(cmake, projectRoot.resolve("CMakeLists.txt"))
    }

    fun generateFiles() {
        if (Files.exists(projectRoot)) {
            // Skipping since project template already exists
            messageReporter.nowhere().info("Skipping federate $targetName as project already exists")
            return
        }

        FileUtil.createDirectoryIfDoesNotExist(projectRoot.toFile())
        val platform = if (federate.platform == PlatformType.Platform.AUTO) targetConfig.get(PlatformProperty.INSTANCE).platform else federate.platform
        when (platform) {
            PlatformType.Platform.NATIVE -> generateFilesNative()
            PlatformType.Platform.ZEPHYR -> generateFilesZephyr()
            PlatformType.Platform.RIOT -> generateFilesRiot()
            else -> messageReporter.nowhere().error("Cannot generate federate templates for platform ${federate.platform.name}")
        }
    }
}
