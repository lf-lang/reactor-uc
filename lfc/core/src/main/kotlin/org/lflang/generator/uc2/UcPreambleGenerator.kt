package org.lflang.generator.uc2

import org.lflang.generator.PrependOperator
import org.lflang.ir.File
import org.lflang.ir.Preamble

class UcPreambleGenerator(
    private val resource: File,
    private val fileConfig: UcFileConfig,
) {
  /** A list of all preambles defined in the resource (file) */
  private val preambles: List<Preamble> = resource.preambles
  private val includeGuard = "LF_GEN_${resource.name.uppercase()}_PREAMBLE_H"

  fun generateHeader(): String {
    val importedResources = resource.imports
    val includes =
        importedResources.map { """#include "${fileConfig.getPreambleHeaderPath(it.file)}"""" }

    return with(PrependOperator) {
      """
                |#ifndef ${includeGuard}
                |#define ${includeGuard}
                |
                |#include "reactor-uc/reactor-uc.h"
            ${" |"..includes.joinToString(separator = "\n", prefix = "// Include the preambles from imported files \n")}
                |
            ${" |"..preambles.joinToString(separator = "\n") { it.code.code }}
                |#endif // ${includeGuard}
            """
          .trimMargin()
    }
  }
}
