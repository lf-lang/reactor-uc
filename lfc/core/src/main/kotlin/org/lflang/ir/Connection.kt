package org.lflang.ir

enum class ConnectionKind {
  CONNECTION,
  DELAYED_CONNECTION,
  PHYSICAL_CONNECTION,
  FEDERATED_CONNECTION,
}

class Connection(
    val sourceRef: TriggerRef,
    val targetPortRefs: List<TriggerRef>,
    val kind: ConnectionKind,
    val delay: TimeValue,
    val isIterated: Boolean,
    val bufferSize: Int,
    val width: Int
) {
  lateinit var source: Port
  lateinit var targets: List<Port>
  lateinit var container: Reactor

  val isPhysical: Boolean
    get() = kind == ConnectionKind.PHYSICAL_CONNECTION

  val isDelayed: Boolean
    get() = kind == ConnectionKind.DELAYED_CONNECTION

  val uid
      get() : Int {
          var uid = 0
          for (conn in container.connections) {
              if (conn == this) {
                  return uid
              }
              uid++
          }
          return -1
      }

    val numDownstreams
      get() = targets.size

    val maxNumPendingEvents
        get() = bufferSize //TODO:

    val isLogical
        get() = !isPhysical && !isDelayed
}
