# Package imports

micro-LF supports a simple package system for managing and importing reusable reactors.
Packages are ordinary directory trees (typically git clones) that follow a standard layout.
The syntax for importing from a package is:

```ulf
import ReactorName from <package/file>
```

Here, `package` is the name of the installed package, and `file` is the name of a `.ulf`
(or `.lf`) file within that package. The compiler resolves this to the file at
`package/src/lib/file`. As with ordinary imports, you may import multiple reactors from the
same file, use aliases, or omit the `as` clause:

```ulf
import Count from <library-test/Import.ulf>

import ReactorA, ReactorB as B from <my-package/Utilities.ulf>
```

The library file name may also be omitted. In that case, the compiler looks for
`src/lib/ReactorName.ulf` in the package (for example, `import Count from <my-package>`
resolves to `my-package/src/lib/Count.ulf`).

## Where packages are installed

When resolving `package/file`, the compiler searches for a directory named `package` in the
following locations, in order:

1. **Project build directory:** `root/build/lfc_include/package`  
   Packages installed by the [Lingo package manager](https://github.com/lf-lang/lingo)
   (via `lingo build`) are placed here.

2. **Project-local packages:** `root/lf-packages/package`  
   You can install a package manually by placing it in an `lf-packages` directory at the root
   of your LF project.

3. **Global packages directory:** `$LF_PACKAGES/package`  
   If the `LF_PACKAGES` environment variable is set, the compiler also searches for packages
   in that directory. This is useful for sharing packages across multiple projects on the
   same machine.

In each case, `root` is the parent directory of the nearest `src` directory that contains
the file with the `import` statement. For example, if your program is in
`my-project/src/Main.ulf`, then `root` is `my-project`.

If the package cannot be found in any of these locations, the compiler reports an error
listing the directories that were searched.

## Obtaining packages

A package follows the standard package layout described below. The
[Lingo package manager](https://github.com/lf-lang/lingo) can also fetch packages from git
repositories and install them into `build/lfc_include`.

To install a package locally without Lingo, clone or copy it into `lf-packages` under your
project root:

```bash
cd my-project
mkdir -p lf-packages
git clone https://github.com/example/some-package.git lf-packages/some-package
```

To make a package available globally, clone packages into a directory and point the environment variable
`LF_PACKAGES` at it:

```bash
export LF_PACKAGES=$HOME/lf-modules
git clone https://github.com/example/some-package.git $LF_PACKAGES/some-package
```

## Standard package layout

An LF package should be structured as follows:

```
<packageName>/
├── README.md            # Package documentation
└── src/
    ├── Demo.ulf         # Example programs (optional)
    └── lib/
        └── Reactors.ulf # Library files containing importable reactors
```

- **`src/lib/`** — Contains library files with reactors that other programs import. These
  are the files referenced by `<package/file>` in an import statement.
- **`README.md`** — Documents what the package provides and how to use it.
- **`src/`** — May contain demo LF programs that illustrate how to use the package. These
  are ordinary source files, not library files.

A package managed by Lingo may also include a `Lingo.toml` manifest at the package root.
