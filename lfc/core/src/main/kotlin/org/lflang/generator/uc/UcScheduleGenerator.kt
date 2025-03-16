package org.lflang.generator.uc

import org.lflang.AttributeUtils.getConnectionBufferSize
import org.lflang.analyses.statespace.StateSpaceExplorer
import org.lflang.generator.PortInstance
import org.lflang.generator.PrependOperator
import org.lflang.generator.ReactionInstance
import org.lflang.generator.ReactorInstance
import org.lflang.lf.Connection
import org.lflang.lf.Port
import org.lflang.pretvm.InstructionGenerator
import org.lflang.pretvm.Label
import org.lflang.pretvm.PartialSchedule
import org.lflang.pretvm.Registers
import org.lflang.pretvm.dag.DagGenerator
import org.lflang.pretvm.instruction.*
import org.lflang.pretvm.register.Register
import org.lflang.pretvm.register.RuntimeVar
import org.lflang.pretvm.register.WorkerRegister
import org.lflang.pretvm.scheduler.LoadBalancedScheduler
import org.lflang.pretvm.scheduler.StaticScheduler
import org.lflang.target.TargetConfig
import java.io.IOException
import java.nio.file.Files
import java.nio.file.Path
import java.util.*

class UcScheduleGenerator(
    private val fileConfig: UcFileConfig,
    private val targetConfig: TargetConfig,
    private val main: ReactorInstance,
    private val reactors: List<ReactorInstance>,
    private val reactions: List<ReactionInstance>,
    private val ports: List<PortInstance>,
    private val connectionGenerator: UcConnectionGenerator
) {
    private val workers: Int = 1
    private val registers = Registers(workers)
    private val graphDir: Path = fileConfig.srcGenPath.resolve("graphs").apply {
        try {
            Files.createDirectories(this)
        } catch (e: IOException) {
            throw RuntimeException(e)
        }
    }
    private val util = UcPlatformUtil(reactors, connectionGenerator)

    fun doGenerate(): List<List<Instruction<*, *, *>>> {
        val SSDs = StateSpaceExplorer.generateStateSpaceDiagrams(main, targetConfig, graphDir)

        val schedules = SSDs.map { ssd -> PartialSchedule().apply { diagram = ssd } }
        PartialSchedule.link(schedules, registers)

        val dagGenerator = DagGenerator(fileConfig)
        val scheduler = createStaticScheduler()

        if (workers == 0) {
            val newWorkers = scheduler.setNumberOfWorkers()
        }

        val instGen = InstructionGenerator(
            fileConfig, targetConfig, workers, main, reactors, reactions, ports, registers, util
        )

        schedules.forEachIndexed { i, schedule ->
            val dag = dagGenerator.generateDag(schedule.diagram)
            require(dag.isValid) { "The generated DAG is invalid: $dag" }

            dag.generateDotFile(graphDir.resolve("dag_raw_$i.dot"))
            dag.removeRedundantEdges()
            dag.generateDotFile(graphDir.resolve("dag_pruned_$i.dot"))

            val dagPartitioned = scheduler.partitionDag(dag, i, workers)
            dagPartitioned.generateDotFile(graphDir.resolve("dag_partitioned_$i.dot"))

            val instructions = instGen.generateInstructions(dagPartitioned, schedule)
            schedule.dag = dagPartitioned
            schedule.instructions = instructions
        }

        val linkedInstructions = instGen.link(schedules, graphDir)
        return linkedInstructions
    }

    /**
     * Generate prepare() and cleanup() functions for delayed connections.
     */
    fun generateHelperFunctions(): String {
        return buildString {
            // Iterate over all reactors
            for (reactor in reactors) {
                for (output in reactor.outputs) {
                    for (srcRange in output.dependentPorts) {
                        for (dstRange in srcRange.destinations) {
                            val input = dstRange.instance
                            with(PrependOperator) { appendLine("|${generateConnectionPrepareFunction(output, input)}".trimMargin()) }
                            with(PrependOperator) { appendLine("|${generateConnectionCleanupFunction(output, input)}".trimMargin()) }
                        }
                    }
                }
            }
            for (worker in 0..< workers) {
                // Generate function for setting temp0 to buffer head's time, before trigger checking.
                with(PrependOperator) { appendLine("|${generateHelperFunctionSetTemp0ToBufferHeadTime(worker)}".trimMargin()) }
            }
        }
    }

    fun generateScheduleCode(instructions: List<List<Instruction<*, *, *>>>): String {
        return buildString {
            // Initialize reactor tags
            val reactorTimeTagInit = reactors.joinToString(", ") {
                "{(Reactor *)&${util.getReactorPointer(it)}, 0}"
            }

            // Generate labels
            with(PrependOperator) {
                appendLine(
                    """
                    |    // Labels
                    |${generateLabels(instructions)}
                    """.trimMargin())
            }

            // FIXME: To support banks and multiports, need to instantiate a buffer per channel.
            with(PrependOperator) {
                appendLine(
                    """
                    |    // Variable declarations
                    |    StaticSchedulerState* state = &((StaticScheduler*)_lf_environment->scheduler)->state;
                    |    const size_t reactor_tags_size = ${reactors.size};
                    |    ReactorTagPair reactor_tags[reactor_tags_size] = {${reactorTimeTagInit}};
                    |    size_t trigger_buffers_size = ${connectionGenerator.getNumDelayedConnections()};
                    |    TriggerBuffer trigger_buffers[trigger_buffers_size];
                    |    
                    |    // Assign start time in the parent scheduler struct
                    |    // and copy it into the PretVM start_time register.
                    |    _lf_environment->scheduler->start_time = _lf_environment->get_physical_time(_lf_environment);
                    |    state->start_time = _lf_environment->scheduler->start_time;
                    """.trimMargin())
            }

            // Initialize event circular buffers.
            val connections = connectionGenerator.getNonFederatedConnections()
            for (i in 0 ..< connectionGenerator.getNumDelayedConnections()) {
                val connection = connections[i]
                val bufferSize = if (getConnectionBufferSize(connection.lfConn) > 0) getConnectionBufferSize(connection.lfConn) else 1
                with(PrependOperator) {
                    appendLine(
                        """
                        |    // Initialize the event circular buffer for connection ${connection.getUniqueName()}.
                        |    cb_init(&trigger_buffers[${i}].buffer, ${bufferSize}, sizeof(Event));
                        |    trigger_buffers[${i}].trigger = (Trigger *)&main_reactor.${connection.getUniqueName()}[0][0];
                        |    trigger_buffers[${i}].staged_event = (Event){0};
                        """.trimIndent()
                    )
                }
            }

            // Generate schedule code.
            with(PrependOperator) {
                appendLine(
                    """
                    |${generatePretVMCode(instructions)}
                    """.trimMargin())
            }

            // Populate the scheduler struct.
            with(PrependOperator) {
                appendLine(
                    """
                    |    // Populate the scheduler struct.
                    |    ((StaticScheduler*)_lf_environment->scheduler)->static_schedule = schedule_0;
                    |    ((StaticScheduler*)_lf_environment->scheduler)->reactor_tags = reactor_tags;
                    |    ((StaticScheduler*)_lf_environment->scheduler)->reactor_tags_size = reactor_tags_size;
                    |    ((StaticScheduler*)_lf_environment->scheduler)->trigger_buffers = trigger_buffers;
                    |    ((StaticScheduler*)_lf_environment->scheduler)->trigger_buffers_size = trigger_buffers_size;
                    """.trimMargin())
            }
        }
    }

    private fun generateLabels(instructions: List<List<Instruction<*, *, *>>>): String {
        return buildString {
            // Generate label macros.
            for (workerId in instructions.indices) {
                val schedule = instructions[workerId]
                for (lineNumber in schedule.indices) {
                    val inst = schedule[lineNumber]
                    // If the instruction already has a label, print it.
                    if (inst.hasLabel()) {
                        val labelList = inst.labels
                        for (label in labelList) {
                            with(PrependOperator) { appendLine("|    static const uint64_t ${getWorkerLabelString(label, workerId)} = ${lineNumber}LL;".trimMargin()) }
                        }
                    } else {
                        val operands = inst.operands
                        for (k in operands.indices) {
                            val operand = operands[k]
                            if (operandRequiresDelayedInstantiation(operand)) {
                                val label =
                                    Label("DELAY_INSTANTIATE_" + inst.opcode + "_" + generateShortUUID())
                                inst.addLabel(label)
                                with(PrependOperator) { appendLine("|    static const uint64_t ${getWorkerLabelString(label, workerId)} = ${lineNumber}LL;".trimMargin()) }
                                break
                            }
                        }
                    }
                }
            }
            with(PrependOperator) { appendLine("|    static const uint64_t PLACEHOLDER = 0LL;".trimMargin()) }
        }
    }

    private fun generatePretVMCode(instructions: List<List<Instruction<*, *, *>>>): String {
        return buildString {
            // Generate static schedules. Iterate over the workers (i.e., the size
            // of the instruction list).
            for (worker in instructions.indices) {
                val schedule = instructions[worker]
                with(PrependOperator) { appendLine("|    inst_t schedule_${worker}[] = {") }
                for (lineNo in schedule.indices) {
                    val inst = schedule[lineNo]

                    // If there is a label attached to the instruction, generate a comment.
                    if (inst.hasLabel()) {
                        val labelList: List<Label> = inst.getLabels()
                        for (label in labelList) {
                            with(PrependOperator) { appendLine("|    // ${getWorkerLabelString(label, worker)}:") }
                        }
                    }

                    // Generate code for each instruction
                    with(PrependOperator) { appendLine("|${generateInstructionCode(lineNo, inst)}") }
                }
                with(PrependOperator) { appendLine("|    };") }
            }
        }
    }

    private fun generateInstructionCode(lineNo: Int, instruction: Instruction<*, *, *>): String {
        return buildString {
            // FIXME: Since reactor-uc does not support threaded execution, there is only one worker.
            val worker = 0
            // Generate code based on opcode
            when (instruction.javaClass.simpleName) {
                "ADD" -> {
                    val inst = instruction as ADD
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)
                    val op3Str = getVarNameOrPlaceholder(inst.operand3, true)
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.reg=(reg_t*)${op3Str}},
                            """
                        )
                    }
                }
                "ADDI" -> {
                    val inst = instruction as ADDI
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)
                    val op3Str = inst.getOperand3()
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.imm=${op3Str}LL},
                            """
                        )
                    }
                }
                "BEQ" -> {
                    val inst = instruction as BEQ
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)
                    val op3Str = getWorkerLabelString(inst.operand3, worker);
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.imm=${op3Str}},
                            """
                        )
                    }
                }
                "BGE" -> {
                    val inst = instruction as BGE
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)
                    val op3Str = getWorkerLabelString(inst.operand3, worker);
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.imm=${op3Str}},
                            """
                        )
                    }
                }
                "BLT" -> {
                    val inst = instruction as BLT
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)
                    val op3Str = getWorkerLabelString(inst.operand3, worker);
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.imm=${op3Str}},
                            """
                        )
                    }
                }
                "BNE" -> {
                    val inst = instruction as BNE
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)
                    val op3Str = getWorkerLabelString(inst.operand3, worker);
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.imm=${op3Str}},
                            """
                        )
                    }
                }
                "DU" -> {
                    val inst = instruction as DU
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true) // hyperperiod offset register
                    val op2Str = inst.operand2  // release time
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.imm=${op2Str}},
                            """
                        )
                    }
                }
                "EXE" -> {
                    val inst = instruction as EXE
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)  // function pointer
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)  // function argument pointer
                    // reactor-uc only: for invoking reaction bodies, op3 is a reactor struct pointer.
                    // For invoking connection helper functions, op3 is null.
                    val op3Str = if (inst.operand3 == null) "NULL" else getVarNameOrPlaceholder(inst.operand3, true)
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.reg=${op3Str}},
                            """
                        )
                    }
                }
                "JAL" -> {
                    val inst = instruction as JAL
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)  // return address
                    val op2Str = getWorkerLabelString(inst.operand2, worker)  // target address
                    val op3Str = if (inst.operand3 == null) "0" else inst.operand3 // offset
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.imm=${op2Str}, .op3.imm=${op3Str}},
                            """
                        )
                    }
                }
                "JALR" -> {
                    val inst = instruction as JALR
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true)  // return address
                    val op2Str = getVarNameOrPlaceholder(inst.operand2, true)  // target address
                    val op3Str = if (inst.operand3 == null) "0" else inst.operand3 // offset
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.reg=(reg_t*)${op2Str}, .op3.imm=${op3Str}},
                            """
                        )
                    }
                }
                "STP" -> {
                    val inst = instruction as STP
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}},
                            """
                        )
                    }
                }
                "WLT" -> {
                    val inst = instruction as WLT
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true) // progress index register
                    val op2Str = inst.operand2  // release value
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.imm=${op2Str}},
                            """
                        )
                    }
                }
                "WU" -> {
                    val inst = instruction as WU
                    val op1Str = getVarNameOrPlaceholder(inst.operand1, true) // progress index register
                    val op2Str = inst.operand2  // release value
                    with(PrependOperator) {
                        appendLine(
                            """
                            |    // Line $lineNo: $inst
                            |    {.func=execute_inst_${inst.opcode}, .opcode=${inst.opcode}, .op1.reg=(reg_t*)${op1Str}, .op2.imm=${op2Str}},
                            """
                        )
                    }
                }
                else -> throw java.lang.RuntimeException("UNREACHABLE: " + instruction.opcode)
            }
        }
    }

    /**
     * Return a C variable name based on the variable type
     * @FIXME: Put it in the platform util.
     */
    private fun getVarName(register: Register): String {
        return when (register.javaClass.simpleName) {
            "BinarySema" -> "binary_sema"
            "ProgressIndex" -> "progress_index"
            "Offset" -> "time_offset"
            "OffsetInc" -> "offset_inc"
            "One" -> "one"
            // "Placeholder" -> "PLACEHOLDER"
            "ReturnAddr" -> "return_addr"
            "StartTime" -> "start_time"
            "Temp0" -> "temp0"
            "Temp1" -> "temp1"
            "Timeout" -> "timeout"
            "Zero" -> "zero"
            else -> throw java.lang.RuntimeException("Unhandled register type: $register")
        }
    }

    /** Return a C variable name based on the variable type  */
    private fun getVarName(registers: List<Register>): String {
        return getVarName(registers[0]) // Use the first register as an identifier.
    }

    /** Return a C variable name based on the variable type  */
    private fun getVarName(register: Register, isPointer: Boolean): String {
        // If it is a runtime variable, return pointer directly.
        if (register is RuntimeVar) return register.pointer
        // Look up the type in getVarName(type).
        val prefix = if (isPointer) "&" else ""
        if (register.isGlobal) {
            return prefix + "state->" + getVarName(register)
        } else {
            val worker = (register as WorkerRegister).owner
            // FIXME: reactor-uc does not yet support threaded execution. So the worker registers are singletons instead of arrays.
            // return prefix + "state->" + getVarName(register) + "[" + worker + "]"
            return prefix + "state->" + getVarName(register)
        }
    }

    /**
     * Return a C variable name based on the variable type. IMPORTANT: ALWAYS use this function when
     * generating the static schedule in C, so that we let the function decide automatically whether
     * delayed instantiation is used based on the type of variable.
     *
     * FIXME: This function should be deprecated for reactor-uc because placeholders are not needed for reactor-uc.
     * Since the schedules are declared inside the main function, there are no compile-time constants.
     */
    private fun getVarNameOrPlaceholder(register: Register, isPointer: Boolean): String {
//        if (operandRequiresDelayedInstantiation(register)) return getPlaceHolderMacroString()
        return getVarName(register, isPointer)
    }

    /**
     * An operand requires delayed instantiation if: 1. it is a RUNTIME_STRUCT register (i.e., fields
     * in the generated LF self structs), or 2. it is a reactor instance. These pointers are not
     * considered "compile-time constants", so the C compiler will complain.
     */
    private fun operandRequiresDelayedInstantiation(operand: Any?): Boolean {
        if ((operand is Register && operand is RuntimeVar)
            || (operand is ReactorInstance)
        ) {
            return true
        }
        return false
    }

    /** Returns the placeholder macro string.  */
    private fun getPlaceHolderMacroString(): String {
        return "PLACEHOLDER"
    }

    /** Generate short UUID to guarantee uniqueness in strings  */
    private fun generateShortUUID(): String {
        return UUID.randomUUID().toString().substring(0, 8) // take first 8 characters
    }

    /** Return a string of a label for a worker  */
    private fun getWorkerLabelString(label: Label, worker: Int): String {
        return "WORKER" + "_" + worker + "_" + label.toString()
    }

    private fun createStaticScheduler(): StaticScheduler {
        return LoadBalancedScheduler(graphDir)
    }

    private fun generateConnectionPrepareFunction(output: PortInstance, input: PortInstance): String {
        val conn = connectionGenerator.getNonFederatedConnections().firstOrNull { connection ->
            val channel = connection.channels[0]
            channel.src.varRef.variable == output.definition &&
                    channel.dest.varRef.variable == input.definition
        } ?: throw RuntimeException("Connection cannot be null.")

        return buildString {
            with(PrependOperator) {
                appendLine("""
                |void ${util.getConnectionPrepareFunction(output, input)}(TriggerBuffer *trigger_buffer) {
                |    CircularBuffer *bufferPtr = &(trigger_buffer->buffer);
                |    cb_pop_front(bufferPtr, &trigger_buffer->staged_event);
                |    main_reactor.conn_source_out_0[0][0]
                |        .super.super.super.prepare(
                |        &main_reactor.conn_source_out_0[0][0], &trigger_buffer->staged_event
                |    );
                |}
            """.trimMargin())
            }
        }
    }


    private fun generateConnectionCleanupFunction(output: PortInstance, input: PortInstance): String {
        val conn = connectionGenerator.getNonFederatedConnections().firstOrNull { connection ->
            val channel = connection.channels[0]
            channel.src.varRef.variable == output.definition &&
                    channel.dest.varRef.variable == input.definition
        } ?: throw RuntimeException("Connection cannot be null.")

        return buildString {
            with(PrependOperator) {
                appendLine("""
                |void ${util.getConnectionCleanupFunction(output, input)}(Trigger *trigger) {
                |    main_reactor.${conn.getUniqueName()}[0][0].super.super.super.cleanup(trigger);
                |}
                """.trimMargin()) }
        }
    }

    private fun generateHelperFunctionSetTemp0ToBufferHeadTime(worker: Int): String {
        return buildString {
            with(PrependOperator) {
                appendLine("""
                |void ${util.helperFunctionSetTemp0ToBufferHeadTime}(TriggerBuffer* tb) {
                |    ((StaticScheduler*)_lf_environment->scheduler)->state.${getVarName(registers.temp0[worker])} = tb->staged_event.tag.time;
                |}
                """.trimMargin()) }
        }
    }
}
