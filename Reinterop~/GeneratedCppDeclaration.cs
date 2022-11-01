﻿namespace Reinterop
{
    internal class GeneratedCppDeclaration
    {
        public GeneratedCppDeclaration(CppType type)
        {
            this.Type = type;
        }

        public CppType Type;
        public List<GeneratedCppDeclarationElement> Elements = new List<GeneratedCppDeclarationElement>();

        public void AddToHeaderFile(CppSourceFile headerFile)
        {
            if (Type == null)
                return;

            string suffix = GetSuffix(headerFile.Includes);

            AddIncludes(headerFile.Includes);
            AddForwardDeclarations(headerFile.ForwardDeclarations);

            CppSourceFileNamespace ns = headerFile.GetNamespace(Type.GetFullyQualifiedNamespace(false));

            string templateDeclaration = "";
            string templateSpecialization = "";
            if (Type.GenericArguments != null && Type.GenericArguments.Count > 0)
            {
                ns.Members.Add(
                    $$"""
                    template <{{string.Join(", ", Type.GenericArguments.Select((CppType type, int index) => "typename T" + index))}}>
                    {{GetTypeKind()}} {{Type.Name}}{{suffix}};
                    """);
                templateDeclaration = "template <> ";
                templateSpecialization = $"<{string.Join(", ", Type.GenericArguments.Select(arg => arg.GetFullyQualifiedName()))}>";
            }

            ns.Members.Add($$"""
                {{templateDeclaration}}{{GetTypeKind()}} {{Type.Name}}{{templateSpecialization}}{{suffix}} {
                  {{GetElements().JoinAndIndent("  ")}}
                };
                """);
        }

        private void AddIncludes(ISet<string> includes)
        {
            foreach (GeneratedCppDeclarationElement element in Elements)
            {
                element.AddIncludesToSet(includes);
            }
        }

        private void AddForwardDeclarations(ISet<string> forwardDeclarations)
        {
            if (Type.GenericArguments != null)
            {
                foreach (CppType genericArg in Type.GenericArguments)
                {
                    genericArg.AddForwardDeclarationsToSet(forwardDeclarations);
                }
            }

            foreach (GeneratedCppDeclarationElement element in Elements)
            {
                element.AddForwardDeclarationsToSet(forwardDeclarations);
            }
        }

        private string GetTypeKind()
        {
            if (Type == null)
                // TODO: report a compiler error instead.
                return "TypeIsNull";

            if (Type.Kind == InteropTypeKind.ClassWrapper || Type.Kind == InteropTypeKind.Delegate)
                return "class";
            else if (Type.Kind == InteropTypeKind.Enum)
                return "enum class";
            else if (Type.Kind == InteropTypeKind.EnumFlags) 
                return "enum";
            else
                return "struct";
        }

        private string GetSuffix(HashSet<string> Includes)
        {
            if (Type == null)
                // TODO: report a compiler error instead.
                return "";

            if (Type.Kind == InteropTypeKind.EnumFlags) {
                // TODO: What if the original C# enum was some other 
                // integral type though?
                Includes.Add("<cstdint>");
                return ": uint32_t";
            } else
                // TODO: support derived classes too? 
                return "";
        }

        private IEnumerable<string> GetElements()
        {
            return Elements.Select(decl =>
            {
                if (decl.Content == null)
                    return "";

                string access;
                if (this.Type.Kind == InteropTypeKind.Enum || this.Type.Kind == InteropTypeKind.EnumFlags)
                    access = "";
                else if (decl.IsPrivate)
                    access = "private: ";
                else
                    access = "public: ";

                return $"{access}{decl.Content}";
            });
        }
    }
}
