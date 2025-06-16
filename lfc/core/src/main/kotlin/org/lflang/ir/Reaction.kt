package org.lflang.ir

data class Reaction(
    /** Index in the containing reactor. */
    val idx: Int,
    /** Target code for the reaction body. */
    val body: TargetCode,

    val triggers: List<TriggerRef>,
    val uses: List<TriggerRef>,
    val effects: List<TriggerRef>,

    /** Location metadata. */
    val loc: LocationInformation,
) {

}