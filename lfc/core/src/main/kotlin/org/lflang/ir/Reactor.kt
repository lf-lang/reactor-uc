package org.lflang.ir

data class TargetCode(val code: String)

data class StateVariable(
    /**
     * Identifier of the state var. From within a reaction
     * body, the state var is accessible as a field with type
     * [type] declared on the `self` struct.
     */
    val lfName: String,
    /** Rust static type of the struct field. Must be `Sized`. */
    val type: TargetCode,
    /** The field initializer, a Rust expression. */
    val init: TargetCode
)

data class ConstructorParameters(
    val lfName: String,
    val type: TargetCode,
    val defaultValue: TargetCode?,
    val isTime: Boolean,
    val defaultValueAsTimeValue: TimeValue?,
)


class Reactor (
    /** Name of the reactor in LF. By LF conventions, this is a PascalCase identifier. */
    val lfName: String,

    val fullyQualifiedName: String,

    /** Whether this is the main reactor. */
    val isMain: Boolean,

    /** List of preambles, will be outputted at the top of the file. */
    val preambles: List<TargetCode>,

    /** List of ctor parameters. */
    val ctorParams: List<ConstructorParameters>,
    /** List of state variables. */
    val stateVars: List<StateVariable>,

    /** Other reactor components, like actions, timers, port references, ports. */
    val triggers: Set<Trigger>,
    // TODO: maybe split this into Input and Output ports
    val ports: List<Port>,

    /** A list of reactions, in order of their [ReactionInfo.idx]. */
    val reactions: List<Reaction>,

    //val typeParamList: List<TypeParamInfo>,

    /**
     * List of reactor instances that are directly created by this reactor.
     * Each of them is a named item accessible from the assemble method (well,
     * their ports are anyway).
     */
    val childReactors: List<Reactor>,

    /**
     * List of connections between ports that are made within
     * the body of this reactor instance.
     */
    val connections: List<Connection>,
    val location: LocationInformation,

    ) {
}