package org.lflang.generator.uc

import com.google.common.collect.Iterables
import org.eclipse.emf.ecore.EObject
import org.eclipse.xtext.xbase.lib.IterableExtensions
import org.eclipse.xtext.xbase.lib.IteratorExtensions
import org.lflang.ast.ASTUtils
import org.lflang.generator.PrependOperator
import org.lflang.generator.ReactorInstance
import org.lflang.generator.uc.UcReactorGenerator.Companion.codeType
import org.lflang.graph.InstantiationGraph
import org.lflang.lf.Instantiation
import org.lflang.lf.LfFactory
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.target.TargetConfig
import org.lflang.target.property.FastProperty
import org.lflang.target.property.KeepaliveProperty
import org.lflang.target.property.TimeOutProperty
import org.lflang.toUnixString

abstract class UcMainGenerator(val targetConfig: TargetConfig) {
    abstract fun generateStartSource(): String
    fun getDuration() = if (targetConfig.isSet(TimeOutProperty.INSTANCE)) targetConfig.get(TimeOutProperty.INSTANCE).toCCode() else "FOREVER"
    fun keepAlive() = if(targetConfig.isSet(KeepaliveProperty.INSTANCE)) "true" else "false"
    fun fast() = if(targetConfig.isSet(FastProperty.INSTANCE)) "true" else "false"

    /**
     * Create a new instantiation graph. This is a graph where each node is a Reactor (not a
     * ReactorInstance) and an arc from Reactor A to Reactor B means that B contains an instance of A,
     * constructed with a statement like `a = new A();` After creating the graph, sort the
     * reactors in topological order and assign them to the reactors class variable. Hence, after this
     * method returns, `this.reactors` will be a list of Reactors such that any reactor is
     * preceded in the list by reactors that it instantiates.
     */
    protected fun setReactorsAndInstantiationGraph(fileConfig: UcFileConfig, mainDef: Instantiation?) : List<Reactor> {
        // Build the instantiation graph .
        val instantiationGraph = InstantiationGraph(fileConfig.resource, false)

        // Topologically sort the reactors such that all of a reactor's instantiation dependencies occur
        // earlier in
        // the sorted list of reactors. This helps the code generator output code in the correct order.
        // For example if {@code reactor Foo {bar = new Bar()}} then the definition of {@code Bar} has
        // to be generated before
        // the definition of {@code Foo}.
        val reactors = instantiationGraph.nodesInTopologicalOrder()

        // If there is no main reactor or if all reactors in the file need to be validated, then make
        // sure the reactors
        // list includes even reactors that are not instantiated anywhere.
        if (mainDef == null) {
            val nodes: Iterable<EObject> =
                IteratorExtensions.toIterable(fileConfig.resource.getAllContents())
            for (r in IterableExtensions.filter<Reactor>(nodes, Reactor::class.java)) {
                if (!reactors.contains(r)) {
                    reactors.add(r)
                }
            }
        }

        return reactors
    }

    /**
     * If there is a main or federated reactor, then create a synthetic Instantiation for that
     * top-level reactor and set the field mainDef to refer to it.
     */
    protected fun createMainInstantiation(fileConfig: UcFileConfig) : Instantiation? {
        // Find the main reactor and create an AST node for its instantiation.
        var mainDef : Instantiation? = null
        val nodes: Iterable<EObject> =
            IteratorExtensions.toIterable(fileConfig.resource.getAllContents())
        for (reactor in Iterables.filter(nodes, Reactor::class.java)) {
            if (reactor.isMain) {
                // Creating a definition for the main reactor because there isn't one.
                mainDef = LfFactory.eINSTANCE.createInstantiation()
                mainDef.setName(reactor.name)
                mainDef.setReactorClass(reactor)
            }
        }
        return mainDef
    }

    fun generateStartHeader() = with(PrependOperator) {
        """
            |#ifndef REACTOR_UC_LF_MAIN_H
            |#define REACTOR_UC_LF_MAIN_H
            |
            |void lf_start(void);
            |
            |#endif
            |
        """.trimMargin()
    }

    fun generateMainSource() = with(PrependOperator) {
        """
            |#include "lf_start.h"
            |int main(void) {
            |  lf_start();
            |  return 0;
            |}
        """.trimMargin()
    }
}
class UcMainGeneratorNonFederated(
    private val main: Reactor,
    targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
): UcMainGenerator(targetConfig) {

    private val ucParameterGenerator = UcParameterGenerator(main)
    private var mainDef: Instantiation? = createMainInstantiation(fileConfig);
    var reactors: List<Reactor> = setReactorsAndInstantiationGraph(fileConfig, mainDef)
    var mainInstance: ReactorInstance = ASTUtils.createMainReactorInstance(mainDef, reactors)
    val connections = UcConnectionGenerator(main, null, emptyList())
    private val scheduleGenerator = UcScheduleGenerator(fileConfig, targetConfig, mainInstance, ASTUtils.allReactorInstances(mainInstance), ASTUtils.allReactionInstances(mainInstance), ASTUtils.allPortInstances(mainInstance), connections)

    override fun generateStartSource() = with(PrependOperator) {
        var linkedInstructions = scheduleGenerator.doGenerate()
        """
            |#include "reactor-uc/reactor-uc.h"
            |#include "${fileConfig.getReactorHeaderPath(main).toUnixString()}"
            |static ${main.codeType} main_reactor;
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |//// Static schedule helper functions
            |${scheduleGenerator.generateHelperFunctions()}
            |void lf_start(void) {
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor);
            |    ${main.codeType}_ctor(&main_reactor, NULL, &lf_environment ${ucParameterGenerator.generateReactorCtorDefaultArguments()});
            |    lf_environment.scheduler->duration = ${getDuration()};
            |    lf_environment.scheduler->keep_alive = ${keepAlive()};
            |    lf_environment.fast_mode = ${fast()};
            |    lf_environment.assemble(&lf_environment);
            |    
            |    //// Static schedule
            |${scheduleGenerator.generateScheduleCode(linkedInstructions)}
            |    
            |    lf_environment.start(&lf_environment);
            |    lf_exit();
            |}
        """.trimMargin()
    }
}
class UcMainGeneratorFederated(
    private val currentFederate: UcFederate,
    private val otherFederates: List<UcFederate>,
    targetConfig: TargetConfig,
    private val fileConfig: UcFileConfig,
): UcMainGenerator(targetConfig) {

    private val top = currentFederate.inst.eContainer() as Reactor
    private val ucConnectionGenerator = UcConnectionGenerator(top, currentFederate, otherFederates)
    override fun generateStartSource() = with(PrependOperator) {
        """
            |#include "reactor-uc/reactor-uc.h"
            |#include "lf_federate.h"
            |static ${currentFederate.codeType} main_reactor;
            |static Environment lf_environment;
            |Environment *_lf_environment = &lf_environment;
            |void lf_exit(void) {
            |   Environment_free(&lf_environment);
            |}
            |void lf_start(void) {
            |    Environment_ctor(&lf_environment, (Reactor *)&main_reactor);
            |    lf_environment.scheduler->duration = ${getDuration()};
            |    lf_environment.scheduler->keep_alive = ${keepAlive()};
            |    lf_environment.scheduler->leader = ${top.instantiations.first() == currentFederate.inst && currentFederate.bankIdx == 0};
            |    lf_environment.fast_mode = ${fast()};
            |    lf_environment.has_async_events  = ${currentFederate.inst.reactor.inputs.isNotEmpty()};
            |    ${currentFederate.codeType}_ctor(&main_reactor, NULL, &lf_environment);
            |    lf_environment.net_bundles_size = ${ucConnectionGenerator.getNumFederatedConnectionBundles()};
            |    lf_environment.net_bundles = (FederatedConnectionBundle **) &main_reactor._bundles;
            |    lf_environment.assemble(&lf_environment);
            |    lf_environment.start(&lf_environment);
            |    lf_exit();
            |}
        """.trimMargin()
    }
}
