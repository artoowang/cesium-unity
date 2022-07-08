﻿using System.Collections.Immutable;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;

namespace Oxidize;

[Generator]
public class OxidizeGenerator : IIncrementalGenerator
{
    private static readonly DiagnosticDescriptor RandomWarning = new DiagnosticDescriptor(id: "RANDOM001",
                                                                                              title: "Some random warning",
                                                                                              messageFormat: "Yep something went wrong '{0}'",
                                                                                              category: "SomeCategory",
                                                                                              DiagnosticSeverity.Warning,
                                                                                              isEnabledByDefault: true);

    public void Initialize(IncrementalGeneratorInitializationContext context)
    {
        // For each method in the Oxidize class, get a list of GenerationItems
        IncrementalValuesProvider<IEnumerable<GenerationItem>> perMethodGenerationItems =
            context.SyntaxProvider.CreateSyntaxProvider(
                predicate: IsOxidizeClass,
                transform: GetOxidizeClass);

        // Consolidate the GenerationItems from the different methods
        IncrementalValueProvider<Dictionary<ITypeSymbol, GenerationItem>> generationItems = perMethodGenerationItems.Collect().Select(CombineGenerationItems);

        // Post-process the generation items
        IncrementalValuesProvider<GenerationItem> processedGenerationItems = generationItems.SelectMany(PostProcess);
        
        // Generate the required items
        context.RegisterSourceOutput(processedGenerationItems, Generate);
    }

    private static Dictionary<ITypeSymbol, GenerationItem> CombineGenerationItems(ImmutableArray<IEnumerable<GenerationItem>> listOfItems, CancellationToken token)
    {
        Dictionary<ITypeSymbol, GenerationItem> result = new Dictionary<ITypeSymbol, GenerationItem>(SymbolEqualityComparer.Default);

        foreach (IEnumerable<GenerationItem> items in listOfItems)
        {
            foreach (GenerationItem item in items)
            {
                GenerationItem current;
                if (!result.TryGetValue(item.type, out current))
                {
                    current = new GenerationItem(item.type);
                    result.Add(item.type, current);
                }

                foreach (IMethodSymbol method in item.methods)
                {
                    current.methods.Add(method);
                }

                foreach (IPropertySymbol property in item.properties)
                {
                    current.properties.Add(property);
                }
            }
        }

        return result;
    }

    private static bool IsOxidizeClass(SyntaxNode node, CancellationToken token)
    {
        var classNode = node as ClassDeclarationSyntax;
        if (classNode == null)
        {
            return false;
        }

        var className = classNode.Identifier.ValueText;
        if (className != "Oxidize")
        {
            return false;
        }

        return true;
    }

    private static IEnumerable<GenerationItem> GetOxidizeClass(GeneratorSyntaxContext ctx, CancellationToken token)
    {
        SemanticModel semanticModel = ctx.SemanticModel;
        OxidizeWalker walker = new OxidizeWalker(semanticModel);

        var classSyntax = (ClassDeclarationSyntax)ctx.Node;
        foreach (MemberDeclarationSyntax memberSyntax in classSyntax.Members)
        {
            MethodDeclarationSyntax? methodSyntax = memberSyntax as MethodDeclarationSyntax;
            if (methodSyntax == null)
                continue;

            walker.Visit(methodSyntax);
        }

        return walker.GenerationItems.Values;
    }

    private static IEnumerable<GenerationItem> PostProcess(Dictionary<ITypeSymbol, GenerationItem> items, CancellationToken token)
    {
        foreach (GenerationItem item in items.Values)
        {
            InheritanceChainer.Chain(item, items);
        }

        return items.Values;
    }

    private static void Generate(SourceProductionContext ctx, GenerationItem item)
    {
        Console.WriteLine(item.type.Name);
        if (item.baseClass != null)
        {
            Console.WriteLine("  Base Class: " + item.baseClass.type.Name);
        }
        Console.WriteLine("  Interfaces");
        foreach (GenerationItem anInterface in item.interfaces)
        {
            Console.WriteLine("    " + anInterface.type.Name);
        }
        Console.WriteLine("  Properties");
        foreach (IPropertySymbol property in item.properties)
        {
            Console.WriteLine("    " + property.Name);
        }
        Console.WriteLine("  Methods");
        foreach (IMethodSymbol method in item.methods)
        {
            Console.WriteLine("    " + method.Name);
        }
    }
}
