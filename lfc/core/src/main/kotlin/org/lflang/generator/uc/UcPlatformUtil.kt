package org.lflang.generator.uc

import org.lflang.generator.PortInstance
import org.lflang.generator.ReactionInstance
import org.lflang.generator.ReactorInstance
import org.lflang.generator.TriggerInstance
import org.lflang.pretvm.PlatformUtil

/**
 * A utility class for generating strings for fields used in the static schedule on the reactor-uc platform.
 * The additional information required to generate the strings is passed into the parameters.
 */
class UcPlatformUtil(
    private val reactors: List<ReactorInstance>,
    private val connectionGenerator: UcConnectionGenerator
) : PlatformUtil {

    /**
     * For reactor-uc, getting the reactor pointer does not rely on the main reactor. So the first parameter can be null.
     *
     * FIXME: Is this a pointer or struct?
     */
    override fun getReactorPointer(reactor: ReactorInstance): String {
        val fullname = reactor.fullName
        if (!fullname.contains(".")) return "main_reactor"
        else {
            var split = fullname.split(".").toMutableList()
            split[0] = "main_reactor"
            return split.joinToString(".")
        }
    }

    override fun getReactorTimePointer(reactor: ReactorInstance): String {
        return "&(reactor_tags[${reactors.indexOf(reactor)}].tag.time)"
    }

    override fun getPortPointer(port: PortInstance): String {
        return sanitizeMainReactorName(port.getFullNameWithJoiner("->"))
    }

    override fun getPortIsPresentFieldPointer(port: PortInstance): String {
        return "&(${getPortPointer(port)}->super.super.is_present)"
    }

    override fun getReactionFunctionPointer(reaction: ReactionInstance): String {
        return "${getReactorPointer(reaction.parent)}->reaction${reaction.index}.super.body"
    }

    override fun getReactionDeadlineHandlerFunctionPointer(reaction: ReactionInstance): String {
        return "${getReactorPointer(reaction.parent)}->reaction${reaction.index}.super.deadline_handler"
    }

    override fun getReactionFunctionParameter1(reaction: ReactionInstance): String {
        return "&(${getReactorPointer(reaction.parent)}->reaction${reaction.index})"
    }

    override fun getReactionFunctionParameter2(reaction: ReactionInstance): String {
        return "&(${getReactorPointer(reaction.parent)})"
    }

    override fun getPqueueHead(main: ReactorInstance?, trigger: TriggerInstance<*>?): String {
        return "trigger_buffers[0].buffer.count"
    }

    override fun getPqueueHeadTimePointer(port: PortInstance): String {
        val conn = connectionGenerator.getConnectionFromInputPort(port.definition)
        return "((Event*)trigger_buffers[${connectionGenerator.getNonFederatedConnections().indexOf(conn)}].buffer.head)->tag.time"
    }

    override fun getConnectionPrepareFunction(output: PortInstance, input: PortInstance): String {
        return "prepare_connection_${output.getFullNameWithJoiner("_")}_${input.getFullNameWithJoiner("_")}"
    }

    override fun getConnectionPrepareFunctionArgument(output: PortInstance, input: PortInstance): String {
        val connIndex = connectionGenerator.getNonFederatedConnections().indexOfFirst { connection ->
            val channel = connection.channels[0]
            channel.src.varRef.variable == output.definition &&
                    channel.dest.varRef.variable == input.definition
        }
        if (connIndex == -1) throw RuntimeException("Connection cannot be null.")

        return "&trigger_buffers[${connIndex}]"
    }

    /**
     * FIXME: This does not work with multiports.
     */
    override fun getConnectionCleanupFunction(output: PortInstance, input: PortInstance): String {
        return "cleanup_connection_${output.getFullNameWithJoiner("_")}_${input.getFullNameWithJoiner("_")}"
    }

    override fun getConnectionCleanupFunctionArgument(output: PortInstance, input: PortInstance): String {
        val conn = connectionGenerator.getNonFederatedConnections().firstOrNull { connection ->
            val channel = connection.channels[0]
            channel.src.varRef.variable == output.definition &&
                    channel.dest.varRef.variable == input.definition
        } ?: throw RuntimeException("Connection cannot be null.")

        return "&(main_reactor.${conn.getUniqueName()}[0][0])"
    }

    override fun getConnectionCleanupFunctionArgument2(output: PortInstance, input: PortInstance): String {
        return "${getReactorPointer(output.parent)}"
    }

    /**
     * Given a component name of pattern "MainReactorName->reactor->component", replace "MainReactorName->" with "main_reactor."
     */
    private fun sanitizeMainReactorName(str: String) : String {
        var split = str.split("->").toMutableList()
        split.removeAt(0) // Removes the main reactor name, to be replaced with "main_reactor"
        return "main_reactor." + split.joinToString("->")
    }

    override fun getIndexToInsertPrepareFunction(reactionExeIndex: Int): Int {
        return reactionExeIndex
    }
}