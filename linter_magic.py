import clang.cindex
from clang.cindex import CursorKind
import os

def analyze_project(build_dir, project_root, target_functions):
    index = clang.cindex.Index.create()

    try:
        compdb = clang.cindex.CompilationDatabase.fromDirectory(build_dir)
    except clang.cindex.CompilationDatabaseError:
        print("Could not find compile_commands.json in the specified directory.")
        return

    # 1. Set up absolute paths for filtering
    allowed_dirs = (
        os.path.abspath(os.path.join(project_root, 'src')),
        os.path.abspath(os.path.join(project_root, 'include'))
    )

    # 2. State tracker for deduplication
    seen_issues = set()

    for cmd in compdb.getAllCompileCommands():
        filename = cmd.filename

        # 3. Fast exit if the cursor moves outside the target directories
        file_path = os.path.abspath(filename)
        if not file_path.startswith(allowed_dirs):
            continue

        # Extract targeted arguments
        extracted_args = ['-x', 'c++', '-std=c++17']

        # Apply system includes explicitly
        extracted_args.extend([
            '-isystem/usr/lib/llvm-23/lib/clang/23/include',
            '-isystem/usr/local/include',
            '-isystem/usr/include/x86_64-linux-gnu',
            '-isystem/usr/include'
        ])

        raw_args = list(cmd.arguments)
        i = 0
        while i < len(raw_args):
            arg = raw_args[i]
            # Capture -I, -isystem, and -D flags
            if arg.startswith('-I') or arg.startswith('-D') or arg.startswith('-isystem'):
                extracted_args.append(arg)
                # Handle space-separated arguments (e.g., "-I /path")
                if arg in ('-I', '-D', '-isystem') and i + 1 < len(raw_args):
                    extracted_args.append(raw_args[i+1])
                    i += 1
            i += 1

        tu = index.parse(filename, args=extracted_args, options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

        print(f"Analyzing: {filename}")
        detect_literal_parameters(tu.cursor, target_functions, allowed_dirs, seen_issues)

def skip_casts_and_parens(cursor):
    """Recursively traverses the AST to skip unexposed expressions,
       implicit casts, and parentheses."""
    if cursor.kind in (CursorKind.UNEXPOSED_EXPR, CursorKind.PAREN_EXPR):
        # print(cursor.kind, [t.spelling for t in cursor.get_tokens()])
        # Look at the first child of the unexposed expression
        for child in cursor.get_children():
            return skip_casts_and_parens(child)
    return cursor

def detect_literal_parameters(cursor, target_functions, allowed_dirs, seen_issues):
    # if cursor.kind == CursorKind.CALL_EXPR and cursor.spelling in target_functions:
    if cursor.kind == clang.cindex.CursorKind.MACRO_INSTANTIATION:
        # Get the unexpanded macro tokens
        print(cursor.__dict__)
        tokens = [token.spelling for token in cursor.get_tokens()]
        print(f"Macro Expansion Found at {cursor.location.line}:{cursor.location.column}")
        print(f"Original Text: {' '.join(tokens)}")

    # print(cursor.spelling)
    if cursor.kind == CursorKind.CALL_EXPR:
        if cursor.referenced:
            call_name = cursor.referenced.spelling
            # print(call_name)
        else:
            call_name = cursor.spelling

        if call_name in target_functions:
            args = list(cursor.get_arguments())

            # args = [ skip_casts_and_parens(arg) for arg in list(cursor.get_arguments()) ]
            for arg in args:
                location = cursor.location
                issue_id = f"{location.file.name}:{location.line}:{location.column} : {cursor.spelling}"
                # print(issue_id, skip_casts_and_parens(arg).kind)

            if args:
                target_arg = args[0]

                if target_arg.kind == CursorKind.INTEGER_LITERAL:
                    # 4. Generate a unique signature for the finding
                    location = cursor.location
                    issue_id = f"{location.file.name}:{location.line}:{location.column}"

                    # 5. Check and update the deduplication set
                    if issue_id not in seen_issues:
                        seen_issues.add(issue_id)
                        print(f"Literal passed to {cursor.spelling} at {issue_id}")

    for child in cursor.get_children():
        # AST nodes do not always have an associated file
        if child.location.file:
            child_file_path = os.path.abspath(child.location.file.name)
            if not child_file_path.startswith(allowed_dirs):
                continue

        detect_literal_parameters(child, target_functions, allowed_dirs, seen_issues)

def main():
    # opengl_functions = ['glBindImageTexture', 'glBindTexture']
    opengl_functions = ['glBindTexture', 'glBindImageTexture']
    # opengl_functions = ['glBindImageTexture']

    # Pass the current working directory as the project root
    analyze_project('./build/', '.', opengl_functions)

if __name__ == '__main__':
    main()