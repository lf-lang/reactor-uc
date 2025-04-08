package org.lflang.generator.uc

import java.nio.file.Path
import org.lflang.MessageReporter
import org.lflang.generator.CodeMap
import org.lflang.generator.DiagnosticReporting
import org.lflang.generator.ValidationStrategy
import org.lflang.generator.Validator
import org.lflang.util.LFCommand

class UcValidator(
    private val fileConfig: UcFileConfig,
    messageReporter: MessageReporter,
    codeMaps: Map<Path, CodeMap>
) : Validator(messageReporter, codeMaps) {

  private val ucValidationStrategy =
      object : ValidationStrategy {
        override fun getCommand(generatedFile: Path?): LFCommand {
          return LFCommand.get(
              "cargo",
              listOf("clippy", "--message-format", "json-diagnostic-rendered-ansi"),
              true,
              fileConfig.srcGenPkgPath)
        }

        override fun getErrorReportingStrategy() = DiagnosticReporting.Strategy { _, _, _ -> }

        override fun getOutputReportingStrategy() =
            DiagnosticReporting.Strategy { validationOutput, errorReporter, map ->
              for (messageLine in validationOutput.lines()) {
                println(messageLine)
              }
            }

        override fun getPriority(): Int = 0

        override fun isFullBatch(): Boolean = true
      }

  override fun getPossibleStrategies(): Collection<ValidationStrategy> =
      listOf(ucValidationStrategy)

  override fun getBuildReportingStrategies():
      Pair<DiagnosticReporting.Strategy, DiagnosticReporting.Strategy> =
      Pair(
          ucValidationStrategy.errorReportingStrategy, ucValidationStrategy.outputReportingStrategy)
}
