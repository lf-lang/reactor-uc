package org.lflang.ir

import org.lflang.target.property.type.PlatformType

class Federate(
    val maxWait: TimeValue,
    val mainReactor: Reactor,
    val platform: PlatformType.Platform,
    val federatedEnvironment: FederatedEnvironment
) {
    val instantiation get() : FederateInstantiation = federatedEnvironment.instantiation.first {it.federate == this }

}
