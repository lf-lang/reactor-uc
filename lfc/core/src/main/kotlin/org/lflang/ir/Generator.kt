package org.lflang.ir

import org.apache.commons.jxpath.ri.compiler.VariableReference
import org.lflang.allPreambles
import org.lflang.allStateVars
import org.lflang.generator.LocationInfo
import org.lflang.generator.locationInfo
import org.lflang.indexInContainer
import org.lflang.isOfTimeType
import org.lflang.isPhysical
import org.lflang.lf.Action
import org.lflang.lf.Code
import org.lflang.lf.Connection
import org.lflang.lf.Deadline
import org.lflang.lf.Expression
import org.lflang.lf.Initializer
import org.lflang.lf.Input
import org.lflang.lf.MaxWait
import org.lflang.lf.Parameter
import org.lflang.lf.Preamble
import org.lflang.lf.Reaction
import org.lflang.lf.Reactor
import org.lflang.lf.STP
import org.lflang.lf.StateVar
import org.lflang.lf.Time
import org.lflang.lf.Timer
import org.lflang.lf.TriggerRef
import org.lflang.lf.Type
import org.lflang.lf.VarRef
import org.lflang.toText
import kotlin.io.path.Path
import kotlin.time.Duration
import kotlin.time.Duration.Companion.nanoseconds


class ParseContext {
    var reactors: HashMap<String, org.lflang.ir.Reactor> = HashMap()
}

fun fromAst(time: Time) : TimeValue = TimeValue(time.interval.nanoseconds)
fun Expression.toTimeValue() : TimeValue = TimeValue(timeInNanoseconds = Duration.parse(this.toString()))

fun fromXText(type: Type?) : CType {
    return if (type == null) {
        CType(
            targetCode = TargetCode(code = ""),
            isArray = false,
            arrayLength = 0,
            isVoid = false
        )
    } else {
        CType(
            targetCode = type.code.toIR(),
            isArray = type.cStyleArraySpec != null,
            arrayLength = type.cStyleArraySpec!!.length,
            isVoid = false
        )
    }
}


fun Action.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.Action = org.lflang.ir.Action(
    this.name,
    kind = TriggerKind.ACTION,
    isPhysical = isPhysical,
    type = fromXText(this.type),
    maxNumPendingEvents = 10,
    minDelay = this.minDelay.toTimeValue(),
    minSpacing = this.minSpacing.toTimeValue(),
)

fun Timer.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.Timer = org.lflang.ir.Timer(
    lfName = this.name,
    offset = fromAst(this.offset as Time),
    period = fromAst(this.period as Time),
    kind = TriggerKind.TIMER,
)

fun Preamble.toIR() : TargetCode = TargetCode(code=this.code.toString())
fun Code.toIR() : TargetCode = TargetCode(code=this.body.toString())
fun String.toIR(): TargetCode = TargetCode(code=this)

fun Parameter.toIR() : ConstructorParameters = ConstructorParameters(
    lfName = this.name,
    type= TargetCode(code = this.type.toString()),
    defaultValue = this.init.toText().toIR(),
    isTime = this.isOfTimeType,
    defaultValueAsTimeValue = null, //TODO: FIX ME
)

fun Initializer.toIR() : TargetCode = TargetCode(code=this.toString())

fun StateVar.toIR() : StateVariable = StateVariable(
    lfName = this.name,
    type = fromXText(this.type),
    init = this.init.toIR(),
    isInitialized = this.init.isAssign, //TODO: fix
)

fun TriggerRef.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.TriggerRef {
    return when (this.toString()) {
        "startup" -> StartupTriggerRef(
            container = reactor
        )
        "shutdown" -> ShutdownTriggerRef(
            container = reactor
        )
        else -> throw IllegalArgumentException("Unknown BuiltInTrigger Ref '$this'")
    }
}

fun LocationInfo.toIR(): ReactorLocationInformation = ReactorLocationInformation(
    line = this.line,
    column = this.column,
    file = Path(this.fileName),
)

fun STP.toIR() : StpViolationReaction = StpViolationReaction(
    body = this.code.toIR()
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
    stp = this.stp.toIR(),
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

fun VarRef.toIR(container: org.lflang.ir.Reactor) : org.lflang.ir.TriggerRef {
    if (this.variable == null) {
        return VariableNameTriggerRef(
            container = container,
            variable = container.triggers.first { it.lfName == this.variable.name }
        )
    } else {
        val instantiation = container.childReactors.first { it.name == this.variable.name};
        return VariableContainedTriggerRef(
            container = container,
            variable = instantiation.reactor.triggers.first { it.lfName == this.variable.name },
            instance = instantiation.name
        )
    }
}

fun Connection.toIR(container: org.lflang.ir.Reactor) : List<org.lflang.ir.Connection> {
    var conns = mutableListOf<org.lflang.ir.Connection>();
    for (left in this.leftPorts) {
        conns.add(Connection(
            sourceRef = left.toIR(container),
            targetPortRefs = this.rightPorts.map { it.toIR(container) },
            kind = if (this.isPhysical) ConnectionKind.PHYSICAL_CONNECTION else (if (this.delay == null) ConnectionKind.CONNECTION else ConnectionKind.DELAYED_CONNECTION),
            delay = this.delay.toTimeValue(),
            isIterated = this.isIterated,
            bufferSize = 10,
            width = this.leftPorts.size,
        ))
    }
    return conns;
}

fun Input.toIR() : InputPort = InputPort(
    lfName = this.name,
    dataType = fromXText(this.type),
    kind = TriggerKind.INPUT,
    isMultiport = TODO(),
    isInterleaved = TODO(),
    width = TODO(),
)


fun Type.toIR() : TargetCode = TargetCode(code = this.code.toString())

fun Reactor.toIR() : org.lflang.ir.Reactor =
    org.lflang.ir.Reactor(
        lfName = this.name,
        fullyQualifiedName = this.name,
        isMain = this.isMain,
        preambles = this.allPreambles.map { it.toIR() },
        ctorParams = this.parameters.map { it.toIR() },
        stateVars = this.allStateVars.map { it.toIR() },
        childReactors = TODO(),
        location = this.locationInfo().toIR(),
        env = TODO(),
        federate = TODO(),
        parentReactor = TODO(),
        codeType = TODO(),
        includeGuard = TODO(),
    )