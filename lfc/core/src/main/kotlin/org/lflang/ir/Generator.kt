package org.lflang.ir

import org.apache.commons.jxpath.ri.compiler.VariableReference
import org.eclipse.emf.ecore.resource.Resource
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
import org.lflang.lf.Import
import org.lflang.lf.Initializer
import org.lflang.lf.Input
import org.lflang.lf.Model
import org.lflang.lf.Tardy
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
import org.lflang.target.property.type.PlatformType
import org.lflang.toText
import kotlin.collections.iterator
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


fun Action.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.Action = Action(
    this.name,
    kind = TriggerKind.ACTION,
    isPhysical = isPhysical,
    type = fromXText(this.type),
    maxNumPendingEvents = 10,
    minDelay = this.minDelay.toTimeValue(),
    minSpacing = this.minSpacing.toTimeValue(),
)

fun Timer.toIR(reactor: org.lflang.ir.Reactor) : org.lflang.ir.Timer = Timer(
    lfName = this.name,
    offset = fromAst(this.offset as Time),
    period = fromAst(this.period as Time),
    kind = TriggerKind.TIMER,
)

fun Preamble.toIR() : org.lflang.ir.Preamble = Preamble(code=this.code.toIR())
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

fun Tardy.toIR() : TardyDeliveryReaction = TardyDeliveryReaction(
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
    deadline = this.deadline?.toIR(),
    tardy = this.tardy.toIR(),
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

fun Reactor.toIR(environment: Environment) : org.lflang.ir.Reactor =
    Reactor(
        lfName = this.name ?: "Reactor",
        fullyQualifiedName = this.name ?: "Reactor",
        isMain = this.isMain,
        preambles = this.allPreambles.map { it.toIR() },
        ctorParams = this.parameters.map { it.toIR() },
        stateVars = this.allStateVars.map { it.toIR() },
        childReactors = mutableListOf(), //TODO:
        location = this.locationInfo().toIR(),
        env = environment,
        federate = null,
        parentReactor = null,
    )

fun Import.toIR() : org.lflang.ir.Import = Import(
    reactors = TODO(),
    file = TODO()
)

fun Model.toIR(environment: Environment) : File = File(
    name = this.toString(),
    imports = this.imports.map { it.toIR() },
    preambles = this.preambles.map { it.toIR() },
    reactors = this.reactors.map { it.toIR(environment) },
    path = Path("test"),
)

class XTextConverter(val resource: Resource) {
    fun convert() : Pair<Environment, File> {
        var files = mutableListOf<File>()
        var env = Environment()
        for (obj in resource.contents) {
            files.add((obj as Model).toIR(env))
            println(obj.eClass())
        }

        var reactors = files.flatMap { it.reactors }

        env.mainReactor = reactors.get(0)

        return Pair(env, files.get(0))
    }
}
