package org.lflang.generator.uc

import org.lflang.generator.PrependOperator
import org.lflang.generator.uc.UcInstanceGenerator.Companion.codeTypeFederate
import org.lflang.joinWithLn
import org.lflang.lf.Instantiation
import org.lflang.toUnixString
import kotlin.io.path.name

class UcFederatedLaunchScriptGenerator(private val fileConfig: UcFileConfig) {
    private val S = '$' // a little trick to escape the dollar sign with $S
    fun generateLaunchScript(federates: List<UcFederate>): String = with(PrependOperator) {
        """ |#!/bin/env bash
            |
            |set -m
            |shopt -s huponexit
            |cleanup() {
            |  if [ "$S{EXITED_SUCCESSFULLY}" ] ; then
            |   exit 0
            |  else 
            |   printf "Killing federate %s.\n" $S{pids[*]}
            |   # The || true clause means this is not an error if kill fails.
            |   kill $S{pids[@]} || true
            |   exit 1
            |  fi
            |}
            |
            |trap 'cleanup; exit' EXIT
            |
            |# Launch all federates
            |pids=()
        ${" |"..federates.joinWithLn { launchFederate(it) }}
            |
            |# Wait for all federates to finish
            |for pid in "$S{pids[@]}"
            |do
            |   wait $S{pid} || exit $S?
            |done
            |EXITED_SUCCESSFULLY=true
        """.trimMargin()
    }

    fun launchFederate(federate: UcFederate) = with(PrependOperator) {
        """ |echo "#### Launching federate ${federate.codeType}"
            |if [ "${S}1" = "-l" ]; then
            |   ${fileConfig.binPath}/${federate.codeType} | tee ${federate.codeType}.log &
            |else
            |   ${fileConfig.binPath}/${federate.codeType} &
            |fi
            |pids+=($S!)
            |
        """.trimMargin()
    }
}