package org.lflang.ir

import org.lflang.allPreambles
import org.lflang.allStateVars
import org.lflang.generator.LocationInfo
import org.lflang.generator.locationInfo
import org.lflang.generator.uc.UcParameterGenerator.Companion.targetType
import org.lflang.generator.uc.name
import org.lflang.indexInContainer
import org.lflang.isOfTimeType
import org.lflang.lf.Action
import org.lflang.lf.Code
import org.lflang.lf.Connection
import org.lflang.lf.Initializer
import org.lflang.lf.Input
import org.lflang.lf.Parameter
import org.lflang.lf.Preamble
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.StateVar
import org.lflang.lf.Time
import org.lflang.lf.Timer
import org.lflang.lf.TriggerRef
import org.lflang.lf.VarRef
import org.lflang.toText
import javax.management.ConstructorParameters
import kotlin.coroutines.Continuation
import kotlin.time.Duration.Companion.nanoseconds


fun fromAst(time: Time) : TimeValue = TimeValue(time.interval.nanoseconds)

fun Action.toIR() : org.lflang.ir.Action = Action(this.name)

fun Timer.toIR() : org.lflang.ir.Timer = Timer(
    lfName = this.name,
    offset = fromAst(this.offset as Time),
    period = fromAst(this.period as Time)
)

fun Preamble.toIR() : TargetCode = TargetCode(code=this.code.toString())
fun Code.toIR() : TargetCode = TargetCode(code=this.body.toString())
fun String.toIR(): TargetCode = TargetCode(code=this)

fun Parameter.toIR() : org.lflang.ir.ConstructorParameters = ConstructorParameters(
    lfName = this.name,
    type= TargetCode(code = this.targetType.toString()),
    defaultValue = this.init.toText().toIR(),
    isTime = this.isOfTimeType,
    defaultValueAsTimeValue = null, //TODO: FIX ME
)

fun Initializer.toIR() : TargetCode = TargetCode(code=this.toString())

fun StateVar.toIR() : StateVariable = StateVariable(
    lfName = this.name,
    type = TargetCode(code = this.type.toString()),
    init = this.init.toIR()
)

fun TriggerRef.toIR() : org.lflang.ir.TriggerRef = TriggerRef(
    name = this.name
)

fun LocationInfo.toIR(): LocationInformation = LocationInformation(
    file = this.fileName,
    line = this.line,
    column = this.column
)

fun Reaction.toIR() : org.lflang.ir.Reaction = Reaction(
    idx = this.indexInContainer,
    body = this.code.toIR(),
    triggers = this.triggers.map { it.toIR() },
    uses = this.sources.map { it.toIR() },
    effects = this.effects.map { it.toIR() },
    loc = this.locationInfo().toIR(),
)

fun VarRef.toIR() : PortRef = PortRef(
    name = this.name
)

fun Connection.toIR() : List<org.lflang.ir.Connection> {
    var conns = mutableListOf<org.lflang.ir.Connection>();
    for (left in this.leftPorts) {
        for (right in this.rightPorts) {
            conns.add(Connection(
                source = left.toIR(),
                target = right.toIR(),
                kind = if (this.isPhysical) ConnectionKind.PHYSICAL_CONNECTION else (if(this.delay == null) ConnectionKind.CONNECTION else ConnectionKind.DELAYED_CONNECTION),
            ))
        }
    }
    return conns;
}

fun Input.toIR() : InputPort = InputPort(
    lfName = this.name,
    dataType = this.type.toIR(),
    incomingConnections = this.
)


fun Reactor.toIR() : org.lflang.ir.Reactor = Reactor(
    lfName = this.name,
    fullyQualifiedName = this.name,
    isMain = this.isMain,
    preambles = this.allPreambles.map { it.toIR() },
    ctorParams = this.parameters.map { it.toIR() },
    stateVars = this.allStateVars.map { it.toIR() },
    triggers = this.actions.map { it.toIR() } + this.timers.map { it.toIR() } + this.inputs.map { it.toIR() },
    ports = TODO(),
    reactions = this.reactions.map { it.toIR() },
    childReactors = TODO(),
    connections = this.connections.fold(mutableListOf<org.lflang.ir.Connection>()) { (acc, it) -> acc.addAll(it.toIR()); acc },
    location = this.locationInfo().toIR(),
)