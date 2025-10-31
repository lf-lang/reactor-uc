package org.lflang.ir

import kotlin.time.Duration

data class TimeValue(val timeInNanoseconds: Duration) {
  fun toCCode(): String = "NSEC($timeInNanoseconds)"
}

fun String.toTime(): TimeValue = TimeValue(timeInNanoseconds = Duration.parse(this))
