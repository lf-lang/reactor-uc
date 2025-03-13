package org.lflang.pretvm;

import org.lflang.generator.PortInstance;
import org.lflang.generator.ReactionInstance;
import org.lflang.generator.ReactorInstance;
import org.lflang.generator.TriggerInstance;

public interface PlatformUtil {
    public String getReactorPointer(ReactorInstance reactor);

    public String getReactorTimePointer(ReactorInstance reactor);

    public String getPortPointer(PortInstance port);

    public String getPortIsPresentFieldPointer(PortInstance port);

    public String getReactionFunctionPointer(ReactionInstance reaction);

    public String getReactionDeadlineHandlerFunctionPointer(ReactionInstance reaction);

    public String getPqueueHead(ReactorInstance main, TriggerInstance trigger);

    public String getPqueueHeadTimePointer(PortInstance port);

    public String getConnectionPrepareFunction(PortInstance output, PortInstance input);

    public String getConnectionCleanupFunction(PortInstance output, PortInstance input);

}
