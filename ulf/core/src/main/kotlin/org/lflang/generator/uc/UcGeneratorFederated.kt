package org.lflang.generator.uc

import java.nio.file.Path
import java.nio.file.Paths
import java.lang.reflect.Field
import org.eclipse.emf.ecore.resource.Resource
import org.lflang.AttributeUtils
import org.lflang.ast.ASTUtils
import org.lflang.generator.CodeMap
import org.lflang.generator.GeneratorResult
import org.lflang.generator.GeneratorUtils.canGenerate
import org.lflang.generator.LFGeneratorContext
import org.lflang.generator.uc.UcInstanceGenerator.Companion.width
import org.lflang.lf.LfFactory
import org.lflang.lf.Reactor
import org.lflang.reactor
import org.lflang.scoping.LFGlobalScopeProvider
import org.lflang.target.PlatformType
import org.lflang.toDefinition
import org.lflang.util.FileUtil
import org.json.JSONObject
import org.json.JSONArray

class UcGeneratorFederated(context: LFGeneratorContext, scopeProvider: LFGlobalScopeProvider) :
    UcGenerator(context, scopeProvider) {

  private val nonFederatedGenerator = UcGeneratorNonFederated(context, scopeProvider)
  val federates = mutableListOf<UcFederate>()

  private val clockSyncMainState = UcClockSyncMainAttribute()

  // Compute the total number of events and reactions within a federate. This reuses the
  // computation for the non-federated case, but federated connections are also considered.
  fun totalNumEventsAndReactionsFederated(federate: UcFederate): Pair<Int, Int> {
    val eventsFromFederatedConnections = maxNumPendingEvents[mainDef.reactor]!!
    val unaccountedNumberOfFlushReactions = numberOfOutputConnections[mainDef.reactor]!!
    val eventsAndReactionsInFederate =
        nonFederatedGenerator.totalNumEventsAndReactions(federate.inst.reactor)
    return Pair(
        eventsFromFederatedConnections + eventsAndReactionsInFederate.first,
        unaccountedNumberOfFlushReactions + eventsAndReactionsInFederate.second,
    )
  }

  private fun doGenerateFederate(
      resource: Resource,
      context: LFGeneratorContext,
      srcGenPath: Path,
      federate: UcFederate,
  ): GeneratorResult.Status {
    if (!canGenerate(errorsOccurred(), federate.inst, messageReporter, context))
        return GeneratorResult.Status.FAILED

    // generate header and source files for all reactors
    getAllInstantiatedReactors(federate.inst.reactor).map {
      nonFederatedGenerator.generateReactorFiles(it, srcGenPath)
    }

    generateFederateFiles(federate, srcGenPath)

    // Collect the info on the generated sources
    ucSources.addAll(nonFederatedGenerator.ucSources)
    codeMaps.putAll(nonFederatedGenerator.codeMaps)

    for (r in getAllImportedResources(resource)) {
      val generator = UcPreambleGenerator(r, fileConfig, scopeProvider)
      val headerFile = fileConfig.getPreambleHeaderPath(r)
      val preambleCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
      codeMaps[srcGenPath.resolve(headerFile)] = preambleCodeMap
      FileUtil.writeToFile(
          preambleCodeMap.generatedCode,
          srcGenPath.resolve(headerFile),
          true,
      )
    }
    return GeneratorResult.Status.SUCCESS
  }

  // The same UcGeneratorFederated is used to iteratively generated a project for each federate.
  // After generating we need to clear certain state variables before starting with the next
  // federate.
  private fun clearStateFromPreviousFederate() {
    codeMaps.clear()
    ucSources.clear()
    maxNumPendingEvents.clear()
    nonFederatedGenerator.maxNumPendingEvents.clear()
    nonFederatedGenerator.codeMaps.clear()
    nonFederatedGenerator.ucSources.clear()
  }

  // This function creates an instantiation for the top-level main reactor.
  private fun createMainDef() {
    for (reactor in reactors) {
      if (reactor
          .isFederated) { // Note that this will be the "main" top-level reactor. Not each federate.
        this.mainDef = LfFactory.eINSTANCE.createInstantiation()
        this.mainDef.name = reactor.name
        this.mainDef.reactorClass = reactor
        this.clockSyncMainState.setState(reactor)
      }
    }
  }

  private fun generateFederateTemplates() {

    // Generate top-level folder
    val projectsRoot = fileConfig.srcPkgPath.resolve(mainDef.name)
    FileUtil.createDirectoryIfDoesNotExist(projectsRoot.toFile())

    for (ucFederate in federates) {
      val projectTemplateGenerator =
          UcFederatedTemplateGenerator(
              mainDef,
              ucFederate,
              projectsRoot,
              messageReporter,
          )
      projectTemplateGenerator.generateFiles()
    }
  }

  override fun doGenerate(resource: Resource, context: LFGeneratorContext) {
    // When --gen-fed-templates is used, the base class returns early without
    // populating reactors. We need to initialize manually for that case.
    if (context.args.generateFedTemplates) {
      printInfo(context)
      messageReporter.clearHistory()
      ASTUtils.setMainName(fileConfig.resource, fileConfig.name)
      setReactorsAndInstantiationGraph(context.mode)
    } else {
      super.doGenerate(resource, context)
    }
    createMainDef()
    for (inst in getAllFederates()) {
      for (bankIdx in 0..<inst.width) {
        federates.add(UcFederate(inst, bankIdx))
      }
    }

    // Make sure we have a grandmaster
    if (clockSyncMainState.state != UcClockSyncMainState.OFF &&
        federates.isNotEmpty() &&
        federates.none { it.clockSyncParams.grandmaster }) {
      messageReporter
          .nowhere()
          .warning("No clock sync grandmaster specified. Selecting first federate as grandmaster.")
      federates.first().setGrandmaster()
    }

    if (context.args.generateFedTemplates) {
      generateFederateTemplates()
      return
    }

    for (ucFederate in federates) {
      clearStateFromPreviousFederate()

      val srcGenPath =
          if (ucFederate.isBank) {
            fileConfig.srcGenPath.resolve("${ucFederate.inst.name}_${ucFederate.bankIdx}")
          } else {
            fileConfig.srcGenPath.resolve(ucFederate.inst.name)
          }

      val platformGenerator = UcPlatformGeneratorFederated(this, srcGenPath, ucFederate)
      val res = doGenerateFederate(ucFederate.inst.eResource()!!, context, srcGenPath, ucFederate)

      if (res == GeneratorResult.Status.FAILED) {
        return
      } else {
        // generate platform specific files
        platformGenerator.generatePlatformFiles()

        val platform =
            if (ucFederate.platform == PlatformType.Platform.AUTO)
                AttributeUtils.getPlatform(mainDef.reactorClass.toDefinition())
            else ucFederate.platform
        if (platform == PlatformType.Platform.NATIVE && !context.args.noCompile) {

          if (!platformGenerator.doCompile(context)) {
            context.finish(GeneratorResult.Status.FAILED, codeMaps)
            return
          }
        }
      }
    }

    context.finish(GeneratorResult.Status.SUCCESS, codeMaps)
  }

  private fun generateFederateFiles(federate: UcFederate, srcGenPath: Path) {
    // First thing is that we need to also generate the top-level federate reactor files
    nonFederatedGenerator.generateReactorFiles(federate.inst.reactor, srcGenPath)

    // Then we generate a reactor which wraps around the top-level reactor in the federate.
    val generator = UcFederateGenerator(federate, federates, fileConfig, messageReporter)
    val top = federate.inst.eContainer() as Reactor

    // Record the number of events and reactions in this reactor
    maxNumPendingEvents[top] = generator.getMaxNumPendingEvents()
    numberOfOutputConnections[top] = generator.getNumberOfOutputConnections()

    val headerFile = Paths.get("lf_federate.h")
    val sourceFile = Paths.get("lf_federate.c")
    val federateCodeMap = CodeMap.fromGeneratedCode(generator.generateSource())
    ucSources.add(sourceFile)
    codeMaps[srcGenPath.resolve(sourceFile)] = federateCodeMap
    val headerCodeMap = CodeMap.fromGeneratedCode(generator.generateHeader())
    codeMaps[srcGenPath.resolve(headerFile)] = headerCodeMap

    FileUtil.writeToFile(headerCodeMap.generatedCode, srcGenPath.resolve(headerFile), true)
    FileUtil.writeToFile(federateCodeMap.generatedCode, srcGenPath.resolve(sourceFile), true)

    FileUtil.writeToFile(generateFederateJson(federate).toString(2), srcGenPath.resolve(Paths.get("federate.json")), true)
  }

  private fun generateFederateJson(federate: UcFederate): JSONObject {
    val json = JSONObject()
    json.put("name", federate.name)
    val platform: PlatformType.Platform = federate.platform
    json.put("platform", platform.toString())
    json.put("code_type", federate.codeType)
    json.put("is_bank", federate.isBank)
    if (federate.isBank) {
      json.put("bank_idx", federate.bankIdx)
    }
    json.put("maxwait", federate.getMaxWait())
    json.put("joining_policy", federate.getJoiningPolicy().toString())

    val clockSyncParams = JSONObject()
    clockSyncParams.put("disabled", federate.clockSyncParams.disabled)
    clockSyncParams.put("grandmaster", federate.clockSyncParams.grandmaster)
    clockSyncParams.put("period_ns", federate.clockSyncParams.period)
    clockSyncParams.put("max_adj_ns", federate.clockSyncParams.maxAdj)
    clockSyncParams.put("Kp", federate.clockSyncParams.Kp)
    clockSyncParams.put("Ki", federate.clockSyncParams.Ki)
    json.put("clock_sync_params", clockSyncParams)

    val defaultInterface = generateNetworkInterfaceJson(federate.getDefaultInterface())
    json.put("default_interface", defaultInterface)

    val interfaces = JSONArray()
    for (intf in federate.interfaces) {
      interfaces.put(generateNetworkInterfaceJson(intf))
    }
    json.put("interfaces", interfaces)
    return json
  }

  private fun generateNetworkInterfaceJson(intf: UcNetworkInterface): JSONObject {
    val json = JSONObject()
    json.put("name", intf.name)
    json.put("type", intf.type.toString())
    when (intf.type) {
      NetworkChannelType.TCP_IP -> json.put("ip_address", (intf as UcTcpIpInterface).ipAddress.address)
      NetworkChannelType.COAP_UDP_IP -> json.put("ip_address", (intf as UcCoapUdpIpInterface).ipAddress.address)
      NetworkChannelType.S4NOC -> json.put("core", (intf as UcS4NocInterface).core)
      NetworkChannelType.UART -> {
        json.put("uart_device", (intf as UcUARTInterface).uartDevice)
        json.put("baud_rate", intf.baudRate)
        json.put("data_bits", intf.dataBits)
        json.put("parity", intf.parity)
        json.put("stop_bits", intf.stopBits)
        json.put("async", intf.async)
      }
      NetworkChannelType.BLE -> {
        json.put("device_name", (intf as UcBleInterface).deviceName)
        json.put("name", intf.name)
      }
      NetworkChannelType.CUSTOM -> {
        json.put("include", (intf as UcCustomInterface).include)
        if (intf.args != null) {
          json.put("args", intf.args)
        }
      }
      NetworkChannelType.NONE -> {}
    }
    val endpoints = JSONArray()
    for (endpoint in intf.endpoints) {
      endpoints.put(generateNetworkEndpointJson(endpoint))
    }
    json.put("endpoints", endpoints)
    return json
  }

  private fun generateNetworkEndpointJson(endpoint: UcNetworkEndpoint): JSONObject {
    val json = JSONObject()
    when (endpoint.iface.type) {
      NetworkChannelType.TCP_IP -> {
        json.put("ip_address", (endpoint as UcTcpIpEndpoint).ipAddress.address)
        json.put("port", endpoint.port)
      }
      NetworkChannelType.COAP_UDP_IP -> {
        json.put("ip_address", (endpoint as UcCoapUdpIpEndpoint).ipAddress.address)
      }
      NetworkChannelType.S4NOC -> json.put("core", (endpoint as UcS4NocEndpoint).core)
      NetworkChannelType.UART -> {
        json.put("uart_device", (endpoint as UcUARTEndpoint).uart_device)
        json.put("baud_rate", endpoint.baud_rate)
        json.put("data_bits", endpoint.data_bits)
        json.put("parity", endpoint.parity)
        json.put("stop_bits", endpoint.stop_bits)
        json.put("async", endpoint.async)
      }
      NetworkChannelType.BLE -> {
        json.put("device_name", (endpoint as UcBleEndpoint).device_name)
      }
      NetworkChannelType.CUSTOM -> {}
      NetworkChannelType.NONE -> {}
    }
    return json
  }
}
