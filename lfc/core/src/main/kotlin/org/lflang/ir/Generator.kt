package org.lflang.ir

import org.lflang.allPreambles
import org.lflang.allStateVars
import org.lflang.generator.LocationInfo
import org.lflang.generator.locationInfo
import org.lflang.generator.uc.UcParameterGenerator.Companion.targetType
import org.lflang.generator.uc.UcPortGenerator.Companion.arrayLength
import org.lflang.generator.uc.UcPortGenerator.Companion.isArray
import org.lflang.generator.uc.mics.name
import org.lflang.indexInContainer
import org.lflang.isOfTimeType
import org.lflang.isPhysical
import org.lflang.lf.Action
import org.lflang.lf.Code
import org.lflang.lf.Connection
import org.lflang.lf.Deadline
import org.lflang.lf.Initializer
import org.lflang.lf.Input
import org.lflang.lf.MaxWait
import org.lflang.lf.Parameter
import org.lflang.lf.Preamble
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.StateVar
import org.lflang.lf.Time
import org.lflang.lf.Timer
import org.lflang.lf.TriggerRef
import org.lflang.lf.Type
import org.lflang.lf.VarRef
import org.lflang.toText
import kotlin.time.Duration.Companion.nanoseconds


class ParseContext {
    var reactors: HashMap<String, org.lflang.ir.Reactor> = HashMap()
}

fun fromAst(time: Time) : TimeValue = TimeValue(time.interval.nanoseconds)



fun CType.fromXText(type: Type?) : CType {
    if (type == null) {
        CType(
            targetCode = TargetCode(code = ""),
            isArray = false,
            arrayLength = 0,
            isVoid = false
        )
    } else {
        CType(
            targetCode = type.code.toIR(),
            isArray = type.isArray,
            arrayLength = type.arrayLength,
            isVoid = false
        )
    }
}


fun Action.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.Action = Action(
    this.name,
    kind = TriggerKind.ACTION,
    container = null,
    isPhysical = isPhysical,
    type = type.toIR(),
    maxNumPendingEvents = this.,
    minDelay = TODO(),
    minSpacing = TODO(),
)

fun Timer.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.Timer = Timer(
    lfName = this.name,
    offset = fromAst(this.offset as Time),
    period = fromAst(this.period as Time),
    kind = TriggerKind.TIMER,
    container = reactor
)

fun Preamble.toIR() : TargetCode = TargetCode(code=this.code.toString())
fun Code.toIR() : TargetCode = TargetCode(code=this.body.toString())
fun String.toIR(): TargetCode = TargetCode(code=this)

fun Parameter.toIR() : ConstructorParameters = ConstructorParameters(
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

fun TriggerRef.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.TriggerRef {
    return when (this.name) {
        "startup" -> StartupTriggerRef(
            container = reactor
        )
        "shutdown" -> ShutdownTriggerRef(
            container = reactor
        )
        else -> throw IllegalArgumentException("Unknown BuiltInTrigger Ref '$this'")
    }
}

fun VarRef.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.TriggerRef {
    return if (this.container == null) {
        VariableNameTriggerRef(
            container = reactor,
            variable = variable.name
        )
    } else {
        VariableContainedTriggerRef(
            container = reactor,
            variable = variable.name,
            instance = container.name
        )
    }
}

fun LocationInfo.toIR(): LocationInformation = LocationInformation(
    file = this.fileName,
    line = this.line,
    column = this.column
)

fun Reaction.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.Reaction = Reaction(
    idx = this.indexInContainer,
    body = this.code.toIR(),
    triggerRefs = this.triggers.map { it.toIR(reactor) },
    sourcesRefs = this.sources.map { it.toIR(reactor) },
    effectsRefs = this.effects.map { it.toIR(reactor) },
    loc = this.locationInfo().toIR(),
    container = reactor,
    maxWait = this.maxWait?.toIR(),
    deadline = this.deadline?.toIR(),
)

fun MaxWait.toIR() : MaxWaitReaction =
    MaxWaitReaction(
        body = this.code.toIR(),
        maxWait = this.value.toString().toTime(),
    )

fun Deadline.toIR() : DeadlineReaction = DeadlineReaction(
    body = this.code.toIR(),
    deadline = this.delay.toString().toTime(),
)

fun VarRef.toPortRef() : PortRef = PortRef(
    name = this.name,
)

fun Connection.toIR() : List<org.lflang.ir.Connection> {
    var conns = mutableListOf<org.lflang.ir.Connection>();
    for (left in this.leftPorts) {
        for (right in this.rightPorts) {
            conns.add(Connection(
                sourceRef = left.toPortRef(),
                targetRef = right.toPortRef(),
                kind = if (this.isPhysical) ConnectionKind.PHYSICAL_CONNECTION else (if(this.delay == null) ConnectionKind.CONNECTION else ConnectionKind.DELAYED_CONNECTION),
            ))
        }
    }
    return conns;
}

fun Input.toIR() : InputPort = InputPort(
    lfName = this.name,
    dataType = this.type.toIR(),
    kind = TriggerKind.INPUT,
    container = TODO(),
)

fun Input.toRef() : PortRef = PortRef(name = this.name)

fun Type.toIR() : TargetCode = TargetCode(code = this.code.toString())

fun Reactor.toIR() : org.lflang.ir.Reactor =
    Reactor(
        lfName = this.name,
        fullyQualifiedName = this.name,
        isMain = this.isMain,
        preambles = this.allPreambles.map { it.toIR() },
        ctorParams = this.parameters.map { it.toIR() },
        stateVars = this.allStateVars.map { it.toIR() },
        childReactors = TODO(),
        location = this.locationInfo().toIR(),
    )