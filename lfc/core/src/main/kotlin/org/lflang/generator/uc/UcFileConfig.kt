package org.lflang.generator.uc

import org.eclipse.emf.ecore.resource.Resource
import org.lflang.FileConfig
import org.lflang.lf.Reactor
import org.lflang.name
import org.lflang.util.FileUtil
import java.io.IOException
import java.nio.file.Path

class UcFileConfig(resource: Resource, srcGenBasePath: Path, useHierarchicalBin: Boolean) :
    FileConfig(resource, srcGenBasePath, useHierarchicalBin) {

    @Throws(IOException::class)
    override fun doClean() {
        super.doClean()
        this.ucBuildDirectories.forEach { FileUtil.deleteDirectory(it) }
    }

    val ucBuildDirectories = listOf<Path>(
        this.outPath.resolve("build"),
    )

    /** Relative path to the directory where all source files for this resource should be generated in. */
    private fun getGenDir(r: Resource): Path = this.getDirectory(r).resolve(r.name)

    /** Path to the header file corresponding to this reactor */
    fun getReactorHeaderPath(r: Reactor): Path = getGenDir(r.eResource()).resolve("${r.name}.h")

    /** Path to the source file corresponding to this reactor (needed for non generic reactors)  */
    fun getReactorSourcePath(r: Reactor): Path = getGenDir(r.eResource()).resolve("${r.name}.c")

    /** Path to the build directory containing CMake-generated files */
    val buildPath: Path get() = this.outPath.resolve("build")
}