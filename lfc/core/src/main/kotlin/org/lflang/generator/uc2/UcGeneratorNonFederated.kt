package org.lflang.generator.uc2

import org.eclipse.emf.ecore.resource.Resource
import org.lflang.MessageReporter
import org.lflang.generator.GeneratorResult
import org.lflang.generator.LFGeneratorContext
import org.lflang.generator.TargetTypes
import org.lflang.ir.Environment
import org.lflang.ir.Federate
import org.lflang.ir.File
import org.lflang.ir.Reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.Target
import org.lflang.util.FileUtil
import java.nio.file.Path


class UcGeneratorNonFederated(
    val env: Environment,
    val mainReactor: Reactor,
    val messageReporter: MessageReporter,
    context: LFGeneratorContext,
    scopeProvider: LFGlobalScopeProvider
) : UcGenerator(context, scopeProvider) {
    lateinit var file: File

  override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
    println("UCGenerator !!!");
    val res =
        doGenerateReactor(
            file,
            context,
            fileConfig.srcGenBasePath,
        )

    if (res == GeneratorResult.Status.GENERATED) {
      // generate platform specific files
      //TODO: val platformGenerator = UcPlatformGeneratorNonFederated(this, srcGenPath,)
      // platformGenerator.generatePlatformFiles()

      // TODO:
      // platformGenerator.doCompile(context)
      // context.finish(GeneratorResult.Status.COMPILED, codeMaps)

      //return GeneratorResult.GENERATED_NO_EXECUTABLE as GeneratorResult
    } else {
      //return GeneratorResult.FAILED
    }
  }

    fun doGenerateReactor(
        file: File,
        context: LFGeneratorContext,
        srcGenPath: Path,
    ): GeneratorResult.Status {
        if (context.args.generateFedTemplates) {
            messageReporter
                .nowhere()
                .error("Cannot generate federate templates for non-federated program")
            return GeneratorResult.Status.FAILED
        }

        // Generate header and source files for all instantiated reactors.
        getAllInstantiatedReactors(mainReactor).map { generateReactorFiles(it, srcGenPath) }

        // Generate header and source files for the main reactor.
        generateReactorFiles(mainReactor, srcGenPath)

        // Generate preambles for all reactors.
        generateAllPreambles(file, srcGenPath)

        return GeneratorResult.Status.GENERATED
    }

    fun generateAllPreambles(file: File, srcGenPath: Path) {
        var files: MutableSet<File> = mutableSetOf();
        getAllImportedResources(file, files)
        for (file in files) {
            for (reactor in file.reactors) {
                val generator = UcPreambleGenerator(file, fileConfig)
                val preambleHeaderPath = fileConfig.getPreambleHeaderPath(file)
                FileUtil.writeToFile(generator.generateHeader(), srcGenPath.resolve(preambleHeaderPath), true)
            }
        }
    }

  fun generateReactorFiles(reactor: Reactor, srcGenPath: Path) {
      val generator = UcReactorGenerator(reactor, fileConfig)

      // paths to source files
      val headerFile = fileConfig.getReactorHeaderPath(reactor)
      val sourceFile = fileConfig.getReactorSourcePath(reactor)
      ucSources.add(sourceFile)

      // write to file
      FileUtil.writeToFile(generator.generateSource(), srcGenPath.resolve(headerFile), true)
      FileUtil.writeToFile(generator.generateHeader(), srcGenPath.resolve(sourceFile), true)
  }

    override fun getTargetTypes(): TargetTypes? {
        TODO("Not yet implemented")
    }

    override fun getTarget(): Target? {
        TODO("Not yet implemented")
    }
}
