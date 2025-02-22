package org.lflang.generator.uc

import java.io.IOException
import java.nio.file.Files
import java.nio.file.Path
import org.lflang.analyses.statespace.StateSpaceExplorer
import org.lflang.generator.PortInstance
import org.lflang.generator.ReactionInstance
import org.lflang.generator.ReactorInstance
import org.lflang.pretvm.InstructionGenerator
import org.lflang.pretvm.PartialSchedule
import org.lflang.pretvm.Registers
import org.lflang.pretvm.dag.DagGenerator
import org.lflang.pretvm.scheduler.LoadBalancedScheduler
import org.lflang.pretvm.scheduler.StaticScheduler
import org.lflang.target.TargetConfig

class UcScheduleGenerator(
    private val fileConfig: UcFileConfig,
    private val targetConfig: TargetConfig,
    private val main: ReactorInstance,
    private val reactors: List<ReactorInstance>,
    private val reactions: List<ReactionInstance>,
    private val ports: List<PortInstance>
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

    fun doGenerate() {
        val SSDs = StateSpaceExplorer.generateStateSpaceDiagrams(main, targetConfig, graphDir)

        val schedules = SSDs.map { ssd -> PartialSchedule().apply { diagram = ssd } }
        PartialSchedule.link(schedules, registers)

        val dagGenerator = DagGenerator(fileConfig)
        val scheduler = createStaticScheduler()

        if (workers == 0) {
            val newWorkers = scheduler.setNumberOfWorkers()
        }

        val instGen = InstructionGenerator(
            fileConfig, targetConfig, workers, main, reactors, reactions, ports, registers
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
        instGen.generateCode(linkedInstructions)
    }

    private fun createStaticScheduler(): StaticScheduler {
        return LoadBalancedScheduler(graphDir)
    }
}
