package org.lflang.generator.uc

import org.eclipse.emf.ecore.resource.Resource
import org.lflang.generator.*
import org.lflang.generator.GeneratorUtils.canGenerate
import org.lflang.generator.LFGeneratorContext.Mode
import org.lflang.isGeneric
import org.lflang.lf.Instantiation
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.Target
import org.lflang.target.property.*
import org.lflang.target.property.type.PlatformType
import org.lflang.util.FileUtil
import java.io.IOException
import java.nio.file.Files
import java.nio.file.Path

@Suppress("unused")
class UcGenerator(
    val context: LFGeneratorContext,
    private val scopeProvider: LFGlobalScopeProvider
) :
    GeneratorBase(context) {

    // keep a list of all source files we generate
    val ucSources = mutableListOf<Path>()
    val codeMaps = mutableMapOf<Path, CodeMap>()

    val fileConfig: UcFileConfig = context.fileConfig as UcFileConfig
    val platform = targetConfig.get(PlatformProperty.INSTANCE)

    companion object {
        const val libDir = "/lib/c"
        const val MINIMUM_CMAKE_VERSION = "3.5"
    }

    // Returns a possibly empty list of the federates in the current program
    private fun getAllFederates(): List<Instantiation> {
        val res = mutableListOf<Instantiation>()
        for (reactor in reactors) {
            if (reactor.isFederated) {
                res.addAll(reactor.instantiations)
            }
        }
        return res
    }

    fun doGenerateTopLevel(resource: Resource, context: LFGeneratorContext, srcGenPath: Path) {
        if (!canGenerate(errorsOccurred(), mainDef, messageReporter, context)) return

        // create a platform-specific generator
        val platformGenerator: UcPlatformGenerator = getPlatformGenerator(srcGenPath)

        // generate all core files
        generateFiles(srcGenPath, getAllImportedResources(resource))

        // generate platform specific files
        platformGenerator.generatePlatformFiles()

        // We only invoke CMake on the generated sources when we are targeting POSIX. If not
        // it is the users responsibility to integrate this into the build system of the target
        // platform.
        if (platform.platform != PlatformType.Platform.NATIVE) {
            println("Exiting before invoking target compiler.")
            context.finish(GeneratorResult.GENERATED_NO_EXECUTABLE.apply(context, codeMaps))
        } else if (context.mode == Mode.LSP_MEDIUM) {
            context.reportProgress(
                "Code generation complete. Validating generated code...", IntegratedBuilder.GENERATED_PERCENT_PROGRESS
            )
            if (platformGenerator.doCompile(context)) {
                UcValidator(fileConfig, messageReporter, codeMaps).doValidate(context)
                context.finish(GeneratorResult.GENERATED_NO_EXECUTABLE.apply(context, codeMaps))
            } else {
                context.unsuccessfulFinish()
            }
        } else {
            context.reportProgress(
                "Code generation complete. Compiling...", IntegratedBuilder.GENERATED_PERCENT_PROGRESS
            )
            if (platformGenerator.doCompile(context)) {
                context.finish(GeneratorResult.Status.COMPILED, codeMaps)
            } else {
                context.unsuccessfulFinish()
            }
        }
    }


    override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
        super.doGenerate(resource, context)

        val federates = getAllFederates();
        if (federates.isEmpty()) {
            doGenerateTopLevel(resource, context, fileConfig.srcGenPath)
        } else {
            for (federate in federates) {
                mainDef = federate
                doGenerateTopLevel(resource, context, fileConfig.srcGenPath.resolve(federate.name))
            }
        }

    }

    /**
     * Copy the contents of the entire src-gen directory to a nested src-gen directory next to the generated Dockerfile.
     */
    private fun copySrcGenBaseDirIntoDockerDir() {
        FileUtil.deleteDirectory(context.fileConfig.srcGenPath.resolve("src-gen"))
        try {
            // We need to copy in two steps via a temporary directory, as the target directory
            // is located within the source directory. Without the temporary directory, copying
            // fails as we modify the source while writing the target.
            val tempDir = Files.createTempDirectory(context.fileConfig.outPath, "src-gen-directory")
            try {
                FileUtil.copyDirectoryContents(context.fileConfig.srcGenBasePath, tempDir, false)
                FileUtil.copyDirectoryContents(tempDir, context.fileConfig.srcGenPath.resolve("src-gen"), false)
            } catch (e: IOException) {
                context.errorReporter.nowhere()
                    .error("Failed to copy sources to make them accessible to Docker: " + if (e.message == null) "No cause given" else e.message)
                e.printStackTrace()
            } finally {
                FileUtil.deleteDirectory(tempDir)
            }
            if (errorsOccurred()) {
                return
            }
        } catch (e: IOException) {
            context.errorReporter.nowhere().error("Failed to create temporary directory.")
            e.printStackTrace()
        }
    }

    private fun fetchReactorUc(version: String) {
        val libPath = fileConfig.srcGenBasePath.resolve("reactor-uc")
        // abort if the directory already exists
        if (Files.isDirectory(libPath)) {
            return
        }
        // clone the reactor-cpp repo and fetch the specified version
        Files.createDirectories(libPath)
        commandFactory.createCommand(
            "git",
            listOf("clone", "-n", "https://github.com/lf-lang/reactor-uc.git", "reactor-uc-$version"),
            fileConfig.srcGenBasePath
        ).run()
        commandFactory.createCommand("git", listOf("checkout", version), libPath).run()
    }

    private fun getAllImportedResources(resource: Resource): Set<Resource> {
        val resources: MutableSet<Resource> = scopeProvider.getImportedResources(resource)
        val importedRresources = resources.subtract(setOf(resource))
        resources.addAll(importedRresources.map { getAllImportedResources(it) }.flatten())
        resources.add(resource)
        return resources
    }

    private fun generateFiles(srcGenPath: Path, resources: Set<Resource>) {
        // generate header and source files for all reactors
        for (r in reactors.filterNot {it.isFederated}) {
            val generator = UcReactorGenerator(r, fileConfig, messageReporter)
            val headerFile = fileConfig.getReactorHeaderPath(r)
            val sourceFile = fileConfig.getReactorSourcePath(r)
            val reactorCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
            if (!r.isGeneric)
                ucSources.add(sourceFile)
            codeMaps[srcGenPath.resolve(sourceFile)] = reactorCodeMap
            val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
            codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

            FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
            FileUtil.writeToFile(reactorCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)
        }


        for (r in resources) {
            val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
            val headerFile = fileConfig.getPreambleHeaderPath(r);
            val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
            codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
            FileUtil.writeToFile(preambleCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
        }
    }

    private fun getPlatformGenerator(srcGenPath: Path) = UcStandaloneGenerator(this, srcGenPath)

    override fun getTarget() = Target.UC

    override fun getTargetTypes(): TargetTypes = UcTypes

}
