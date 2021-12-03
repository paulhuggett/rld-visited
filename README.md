# rld-visited

It’s all about supporting static archives as efficiently as possible!

***
 Placeholder for a description of what this is all about. For now just some random notes to be later turned into something more coherent.
***

### Groups

The pump is primed by adding all of the ticket files that are listed on the command line to the link. The compilations referenced by these files form group #0.

Once symbol resolution has completed on the members of group #0, the linker must continue to resolve any strongly undefined symbols that remain. To satisfy these references, we build a collection of files drawn from the static archives which contain the required definitions.

### Namespace

rld considers all of the names defined by static archive to occupy a single flat namespace. Where the same symbol is defined by multiple archive members, the file with the lowest ordinal will be used.

### Ordinals

rld assigns an “ordinal” value to each file. This value is used to select a definition where a choice must be made. This is used to resolve situations where concurrent processing of the inputs could result in inconsistent output.

In all cases, the source files are compiled with a command such as:

```
cc -c a.c
```

### Example #1

#### Setup

1. Compile the three source files.

    | File | Source code             |
    | ---- | ----------------------- |
    | f.c  | `void f(void) { g(); }` |
    | g.c  | `void g(void) { h(); }` |
    | h.c  | `void h(void) {}`       |

1. Create a static archive.

    ```bash
    ar q liba.a g.o h.o
    ```

    Static archive liba.a now contains g.o and h.o. We’ll refer to these as liba.a(g.o) and liba.a(h.o) respectively.

1. Link.

    ```bash
    rld --entry=f f.o liba.a
    ```

The linker performs a series of passes over the inputs as it works to resolve the symbol references.

#### Processing

We follow the processing of the input groups.

- Group #0

    We start with the f.o ticket file (group #0, ordinal #0). Symbol resolution leaves us with a definition of “f” and a strong undefined symbol of “g”. “g” is defined by liba.a(g.o) so this forms group #1 ordinal #1.

- Group #1

    Symbol resolution processes the compilation referenced by liba.a(g.o). This defines symbol “g” and creates a strong undefined symbol for symbol “h”. “h” is found in liba.a(h.o) so we have group #2 ordinal #2.

- Group 2

    liba.a(h.o) (ordinal #2) defines symbol “h” and leaves us with no strongly unreferenced symbols. The link completes successfully.

#### Summary

<table>
    <thead>
        <tr>
            <th colspan="2">Group 0</th><th colspan="2">Group 1</th><th colspan="2">Group 2</th>
        </tr>
        <tr>
            <th>Name</th><th>Ordinal</th>
            <th>Name</th><th>Ordinal</th>
            <th>Name</th><th>Ordinal</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>f.o</td><td>0</td>
            <td>liba.a(g.o)</td><td>1</td>
            <td>liba.a(h.o)</td><td>2</td>
        </tr>
    </tbody>
</table>

### Example #2: Three Groups

#### Setup

1. Compile the source files

   | File | Source code                  |
   | ---- | ---------------------------- |
   | f.c  | `void f(void) { g(); h(); }` |
   | g.c  | `void g(void) { j(); }`      |
   | h.c  | `void h(void) {}`            |
   | j.c  | `void j(void) {}`            |

1. Create archives

   Use the following commands to create archives liba.a and libb.a.

   ```bash
   ar q liba.a g.o j.o
   ar q libb.a h.o
   ```

   This table summarizes the archives are their contents.
   
   | Archive | File members |
   | ------- | ------------ |
   | liba.a  | g.o j.o      |
   | libb.a  | h.o          |

1. Link

    We link f.o directly and pass the two newly created static archives.
    
   ```bash
   rld --entry=f f.o -la -lb
   ```

#### Processing

We follow the processing of the input groups.

- Group #0

    f.o (ordinal #0) is an object file listed on the command-line so is added to the initial group.  This pass defines symbol “f” and leaves a strong undefined symbol for “g” and “h” on completion. “g” is located in liba.a(g.o), “h” in libb.a(h.o) which are assigned ordinals #1 and #2 respectively.

- Group #1

    We perform symbol resolution on liba.a(g.o) and libb.a(h.o). This pass leaves a strong undefined symbol of symbol “j”. “j” is defined by the compilation referenced from liba.a(j.o). This sole compilation forms group #2.

- Group #2

    Symbol resolution is performed for liba.a(j.o) (ordinal #3). When this pass completes there are no strongly undefined symbols and the link completes successfully.

#### Summary

The table below summarizes the collection of files that were included in the file along with their group and ordinal number.

<table>
    <thead>
        <tr>
            <th colspan="2">Group 0</th><th colspan="2">Group 1</th><th colspan="2">Group 2</th>
        </tr>
        <tr>
            <th>Name</th><th>Ordinal</th>
            <th>Name</th><th>Ordinal</th>
            <th>Name</th><th>Ordinal</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>f.o</td><td>0</td>
            <td>liba.a(g.o)</td><td>1</td>
            <td>liba.a(j.o)</td><td>3</td>
        </tr>
        <tr>
            <td></td><td></td>
            <td>libb.a(h.o)</td><td>2</td>
            <td></td><td></td>
        </tr>
    </tbody>
</table>

