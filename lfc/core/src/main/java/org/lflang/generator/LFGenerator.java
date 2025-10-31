package org.lflang.generator;

import static org.lflang.generator.uc2.UcGeneratorKt.createUcGenerator;

import com.google.common.collect.Lists;
import com.google.inject.Inject;
import com.google.inject.Injector;
import java.nio.file.Path;
import org.eclipse.emf.ecore.resource.Resource;
import org.eclipse.xtext.generator.AbstractGenerator;
import org.eclipse.xtext.generator.IFileSystemAccess2;
import org.eclipse.xtext.generator.IGeneratorContext;
import org.lflang.FileConfig;
import org.lflang.MessageReporter;
import org.lflang.ast.ASTUtils;
import org.lflang.generator.uc2.UcFileConfig;
import org.lflang.generator.uc2.UcGenerator;
import org.lflang.generator.uc2.UcGeneratorNonFederated;
import org.lflang.ir.Environment;
import org.lflang.ir.Federate;
import org.lflang.ir.XTextConverter;
import org.lflang.scoping.LFGlobalScopeProvider;
import org.lflang.target.Target;

/** Generates code from your model files on save. */
public class LFGenerator extends AbstractGenerator {

  @Inject private LFGlobalScopeProvider scopeProvider;
  @Inject private Injector injector;

  // Indicator of whether generator errors occurred.
  protected boolean generatorErrorsOccurred = false;

  /**
   * Create a target-specific FileConfig object
   *
   * @return A FileConfig object in Kotlin if the class can be found.
   * @throws RuntimeException If the file config could not be created properly
   */
  public static FileConfig createFileConfig(
      Resource resource, Path srcGenBasePath, boolean useHierarchicalBin, boolean runtimeSymlink) {

    final Target target = Target.fromDecl(ASTUtils.targetDecl(resource));
    assert target != null;

    //      if (FedASTUtils.findFederatedReactor(resource) != null) {
    //        return new FederationFileConfig(resource, srcGenBasePath, useHierarchicalBin);
    //      }

    return new UcFileConfig(resource, srcGenBasePath, useHierarchicalBin, runtimeSymlink);
  }

  /** Create a generator object for the given target. */
  private UcGenerator createGenerator(Resource resource, LFGeneratorContext context, MessageReporter messageReporter) {
    final Target target = Target.fromDecl(ASTUtils.targetDecl(context.getFileConfig().resource));
    var converter = new XTextConverter(resource);
    var env = converter.convert();

    var generator = createUcGenerator(
            context,
            scopeProvider,
            env.getFirst(),
            env.getFirst().getMainReactor(),
            messageReporter
    );

    var gen1 = (UcGeneratorNonFederated) generator;
    gen1.file = env.getSecond();

    return generator;
  }

  @Override
  public void doGenerate(Resource resource, IFileSystemAccess2 fsa, IGeneratorContext context) {
    assert injector != null;
    final LFGeneratorContext lfContext;
    if (context instanceof LFGeneratorContext) {
      lfContext = (LFGeneratorContext) context;
    } else {
      lfContext = LFGeneratorContext.lfGeneratorContextOf(resource, fsa, context);
    }


    // The fastest way to generate code is to not generate any code.
    if (lfContext.getMode() == LFGeneratorContext.Mode.LSP_FAST) return;

    final MessageReporter messageReporter = lfContext.getErrorReporter();
    if (messageReporter instanceof LanguageServerMessageReporter) {
      ((LanguageServerMessageReporter) messageReporter).publishDiagnostics();
    }

    var generator = createGenerator(resource, lfContext, messageReporter);
    var generator1 = (UcGeneratorNonFederated)generator;
    generatorErrorsOccurred = generator.errorsOccurred();

    generator.doGenerate(resource, lfContext);

  }

  /** Return true if errors occurred in the last call to doGenerate(). */
  public boolean errorsOccurred() {
    return generatorErrorsOccurred;
  }
}
