package org.lflang.generator.uc2

import java.nio.file.Path
import org.lflang.ir.File
import org.lflang.ir.Reactor
import kotlin.io.path.appendText

class UcFileConfig(
    val basePath: Path,
) {

    val srcGenBasePath get() : Path = basePath.resolve("src-gen")

    fun relativeToBasePath(path: Path): Path = path.subtract(basePath).first()

    fun generativePath(path: Path): Path {
        basePath.appendText("src-gen")
        basePath.appendText(relativeToBasePath(path).toString())
        return basePath
    }

  private fun getGenDir(path: Path): Path {
      return  generativePath(path)
  }

    /** Path to the header file corresponding to this reactor */
    fun getReactorHeaderPath(r: Reactor): Path = getGenDir(r.location.file).resolve("${r.lfName}.h")

    fun getReactorSourcePath(r: Reactor): Path = getGenDir(r.location.file).resolve("${r.lfName}.c")

    fun getPreambleHeaderPath(r: File): Path = getGenDir(r.path).resolve("_lf_preamble.h")
}
