package org.lflang.ir

import java.nio.file.Path


class Preamble(
    val code: TargetCode
) {
}

class Import(
    val reactors: List<String>,
    val files: Path
    ) {
}


class File(
    val name: String,
    val path: Path,
    val imports: List<Import>,
    val preambles: List<Preamble>,
    val reactors: List<Reactor>,
) {
}