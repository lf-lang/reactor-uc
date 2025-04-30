package org.lflang.generator.uc

import org.lflang.*
import org.lflang.lf.*

class UcStartupCoordinatorGenerator(
    private val federate: UcFederate,
    private val connectionGenerator: UcConnectionGenerator
) {
  private val numNeighbors = connectionGenerator.getNumFederatedConnectionBundles()
  val numSystemEvents = numNeighbors * 3 + 3
}
