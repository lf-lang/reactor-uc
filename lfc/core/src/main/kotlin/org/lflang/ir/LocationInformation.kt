package org.lflang.ir

import java.nio.file.Path

data class ReactorLocationInformation(
    //val declaredIn: File,
    val line: Int,
    val column: Int,
    val file: Path
)
