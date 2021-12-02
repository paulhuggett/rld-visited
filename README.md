# rld-visited

It’s all about supporting static archives as efficiently as possible!

***
 Placeholder for a description of what this is all about. For now just some random notes to be later turned into something more coherent.
***


rld considers all of the names defined by static archive to occupy a single flat namespace. Where the same symbol is defined by multiple archive members, the file with the lowest ordinal will be used.

In all cases, the source files are compiled with a command such as:

```
cc -c a.c
```

### Example 1

Scenario:

1. Compile

    | File | Source code             |
    | ---- | ----------------------- |
    | f.c  | `void f(void) { g(); }` |
    | g.c  | `void g(void) { h(); }` |
    | h.c  | `void h(void) {}`       |

1. Create archive

    ```
    ar a.a g.o h.o
    ```

    Static archive a.a now contains g.o and h.o. The files in archive a.a are known as a.a(g.o) and a.a(h.o) respectively.

1. Link

    ```
    rld --entry=f f.o arch.a
    ```

The linker performs a series of passes over the inputs as it works to resolve the symbol references,

#### Group 0

The pump is primed by adding all of the ticket files that are listed on the command line to the link. The compilations referenced by these files form Group 0.

Once symbol resolution has completed on the members of Group 0, the linker must continue to resolve any strongly undefined symbols that remain. To satisfy these references, we build a collection of files drawn from the static archives which contain the required definitions. In this case, symbol resolution leaves us with a definition of f and a strong undef of g. g is defined by a.a(g.o) so this forms Group 1.

#### Group 1

Symbol resolution processes the compilation referenced by a.a(g.o). This defines symbol g. We now have no strong undefs, so processing the link completes successfully.


### Example 2

| File | Source code           |
| ---- | --------------------- |
| f.c  | `void f(void) { g(); j(); }` |
| g.c  | `void g(void) { h(); }`      |
| h.c  | `void h(void) {}`            |
| j.c  | `void j(void) {}`            |

Create archives:

```
ar a.a g.o j.o
ar b.a h.o
```

| Archive | File members |
| ------- | ------------ |
| a.a | g.o j.o |
| b.a | h.o |

Link:

```
rld --entry=f f.o a.a b.a
```


| Group 0 | Group 1 | Group 2 |
| ------- | ------- | ------- |
| f.o     | a.a(g.o)| j.o     |
|         | a.a(h.o)|         |

| Group | Action |
| ----- | ------ |
| 0 | f.o is an object file listed on the command-line so is added to the initial group.  This pass defines symbol f and leaves a strong undef g on completion. |
| 1 | Symbol g is available in a.a(g.o) so this file is added to the group. At the end of this pass symbol g is defined. |
|   | There no unresolved symbols, so the link completes successfully. |
