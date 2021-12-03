# rld-visited

It’s all about supporting static archives as efficiently as possible!

***
 Placeholder for a description of what this is all about. For now just some random notes to be later turned into something more coherent.
***

### Namespace

rld considers all of the names defined by static archive to occupy a single flat namespace. Where the same symbol is defined by multiple archive members, the file with the lowest ordinal will be used.

### Ordinals

rld assigns an “ordinal” value to each file. This value is used to select a definition where a choice must be made. This is used to resolve situations where concurrent processing of the inputs could result in inconsistent output.

In all cases, the source files are compiled with a command such as:

```
cc -c a.c
```

### Example 1

Scenario:

1. Compile the source files

    | File | Source code             |
    | ---- | ----------------------- |
    | f.c  | `void f(void) { g(); }` |
    | g.c  | `void g(void) { h(); }` |
    | h.c  | `void h(void) {}`       |

1. Create a static archive

    ```bash
    ar q a.a g.o h.o
    ```

    Static archive a.a now contains g.o and h.o. We’ll refer to these as a.a(g.o) and a.a(h.o) respectively.

1. Link

    ```bash
    rld --entry=f f.o a.a
    ```

The linker performs a series of passes over the inputs as it works to resolve the symbol references.

#### Group #0

The pump is primed by adding all of the ticket files that are listed on the command line to the link. The compilations referenced by these files form Group #0.

Once symbol resolution has completed on the members of Group #0, the linker must continue to resolve any strongly undefined symbols that remain. To satisfy these references, we build a collection of files drawn from the static archives which contain the required definitions. In this case, symbol resolution leaves us with a definition of f and a strong undef of g. g is defined by a.a(g.o) so this forms Group #1.

<table>
    <thead>
        <tr>
            <th colspan="2">Group 0</th>
        </tr>
        <tr>
            <th>Name</th><th>Ordinal</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>f.o</td><td>0</td>
        </tr>
    </tbody>
</table>

#### Group #1

Symbol resolution processes the compilation referenced by a.a(g.o). This defines symbol g. We now have no strong undefs so the link completes successfully.

<table>
    <thead>
        <tr>
            <th colspan="2">Group 0</th><th colspan="2">Group 1</th>
        </tr>
        <tr>
            <th>Name</th><th>Ordinal</th>
            <th>Name</th><th>Ordinal</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>f.o</td><td>0</td>
            <td>a.a(g.o)</td><td>1</td>
        </tr>
        <tr>
            <td></td><td></td>
            <td>a.a(h.o)</td><td>2</td>
        </tr>
    </tbody>
</table>

### Example 2: Three Groups

1. Compile the source files

   | File | Source code           |
   | ---- | --------------------- |
   | f.c  | `void f(void) { g(); h(); }` |
   | g.c  | `void g(void) { j(); }`      |
   | h.c  | `void h(void) {}`            |
   | j.c  | `void j(void) {}`            |

2. Create archives

   Use the following commands to create archives a.a and b.a.

   ```bash
   ar q a.a g.o j.o
   ar q b.a h.o
   ```

   This table summarizes the archives are their contents.
   
   | Archive | File members |
   | ------- | ------------ |
   | a.a     | g.o j.o |
   | b.a     | h.o     |

3. Link

    We link f.o directly and pass the two newly created static archives.
    
   ```bash
   rld --entry=f f.o a.a b.a
   ```

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
            <td>a.a(g.o)</td><td>1</td>
            <td>a.a(j.o)</td><td>3</td>
        </tr>
        <tr>
            <td></td><td></td>
            <td>b.a(h.o)</td><td>2</td>
            <td></td><td></td>
        </tr>
    </tbody>
</table>

#### Group #0

<table>
    <thead>
        <tr>
            <th colspan="2">Group 0</th>
        </tr>
        <tr>
            <th>Name</th><th>Ordinal</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>f.o</td><td>0</td>
        </tr>
    </tbody>
</table>

f.o is an object file listed on the command-line so is added to the initial group.  This pass defines symbol f and leaves a strong undefs for g and h on completion.

g is located in a.a(g.o), h in b.a(h.o). These two compilations form group #1.

#### Group #1

<table>
    <thead>
        <tr>
            <th colspan="2">Group 0</th><th colspan="2">Group 1</th>
        </tr>
        <tr>
            <th>Name</th><th>Ordinal</th>
            <th>Name</th><th>Ordinal</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>f.o</td><td>0</td>
            <td>a.a(g.o)</td><td>1</td>
        </tr>
        <tr>
            <td></td><td></td>
            <td>b.a(h.o)</td><td>2</td>
        </tr>
    </tbody>
</table>

We perform symbol resolution on a.a(g.o) and b.a(h.o) which are assigned ordinals 1 and 2 respectively.

This pass leaves a strong undef of symbol j. j is defined by the compilation referenced from a.a(j.o). This sole compilation forms group #2.

#### Group #2

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
            <td>a.a(g.o)</td><td>1</td>
            <td>a.a(j.o)</td><td>3</td>
        </tr>
        <tr>
            <td></td><td></td>
            <td>b.a(h.o)</td><td>2</td>
            <td></td><td></td>
        </tr>
    </tbody>
</table>

Symbol resolution is performed for a.a(j.o) (ordinal 2).

When this pass completes there are no strongly undefined symbols and the link completes successfully.
