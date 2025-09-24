package org.lflang.generator.uc

import java.io.IOException
import java.nio.file.Path
import org.eclipse.emf.ecore.resource.Resource
import org.lflang.FileConfig
import org.lflang.ir.File
import org.lflang.lf.Reactor
import org.lflang.name
import org.lflang.util.FileUtil

class UcFileConfig(
    resource: Resource,
    srcGenBasePath: Path,
    useHierarchicalBin: Boolean,
    runtimeSymlink: Boolean
) : FileConfig(resource, srcGenBasePath, useHierarchicalBin, runtimeSymlink) {

  @Throws(IOException::class)
  override fun doClean() {
    super.doClean()
    this.ucBuildDirectories.forEach { FileUtil.deleteDirectory(it) }
  }

  val ucBuildDirectories =
      listOf<Path>(
          this.outPath.resolve("build"),
      )

  /**
   * Relative path to the directory where all source files for this resource should be generated in.
   */
  private fun getGenDir(file: File): Path = this.getDirectory(r).resolve(r.name)

  /** Path to the header file with preambles defined for this reactor */
  fun getPreambleHeaderPath(r: Resource): Path = getGenDir(r).resolve("_lf_preamble.h")

  /** Path to the source file corresponding to this reactor (needed for non generic reactors) */
  fun getReactorSourcePath(r: Reactor): Path = getGenDir(r.eResource()).resolve("${r.name}.c")

  /** Path to the header file corresponding to this reactor */
  fun getReactorHeaderPath(r: org.lflang.ir.Reactor): Path =
      getGenDir(r.eResource()).resolve("${r.name}.h")
}
