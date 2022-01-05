# rld-visited

[![CI Build/Test](https://github.com/paulhuggett/rld-visited/actions/workflows/ci.yaml/badge.svg)](https://github.com/paulhuggett/rld-visited/actions/workflows/ci.yaml)

It’s all about supporting static archives as efficiently as possible!

***
Placeholder for a description of what this is all about. For now just some random notes to be later turned into something more coherent.
***

## Positions

Each input is assigned a *position* in a two-dimensional space formed by the list of ticket files on the command line and contents of each of the libraries. The *(x,y)* position in this space comes from the input’s position on the linker command line. 

The position is used to determine which archive member is added to the link in the event that more than one definition of a symbol is discovered. If this occurs, we always choose the compilation from the lexicographically lowest position.

Each directly listed ticket is assigned a location of *(0,n)* where *n* comes from the command-line order. These files are *always* included in the link. Libraries are then assigned an *x* position based on their command-line order and starting with *x=1*. The members of each library are given monotonically increasing values for *y*.

For example, consider the following scenario:

    ar q liba.a h.o j.o
    ar q libb.a k.o m.o n.o
    rld f.o liba.a g.o libb.a

Would assign a position of *(0,0)* to `f.o` and *(0,1)* to `g.o`. We then assign positions to the five library members as below:

<table>
    <tbody>
        <tr>
            <th><em>y=2</em></th>
            <td></td>
            <td></td>
            <td>libb.a(n.o)</td>
        </tr>
        <tr>
            <th><em>y=1</em></th>
            <td>g.o</td>
            <td>liba.a(j.o)</td>
            <td>libb.a(m.o)</td>  
        </tr>
        <tr>
            <th><em>y=0</em></th>
            <td>f.o</td>
            <td>liba.a(h.o)</td>
            <td>libb.a(k.o)</td>
        </tr>
    </tbody>
    <tfoot>
        <tr>
            <td border="0"></td>
            <th>Direct<br><em>x=0</em></th>
            <th>liba.a<br><em>x=1</em></th>
            <th>libb.a<br><em>x=2</em></th>
        </tr>
    </tfoot>
</table>

## Groups

The pump is primed by adding all of the ticket files that are listed on the command line to the link. The compilations referenced by these files form group #0.

During symbol resolution for group #0, we also scan all of the specified archives to discover the available definitions.



Once symbol resolution has completed on the members of group #0, the linker must continue to resolve any strongly undefined symbols that remain. To satisfy these references, we build a collection of files drawn from the static archives which contain the required definitions.

## Namespace

rld considers all of the names defined by static archive to occupy a single flat namespace. Where the same symbol is defined by multiple archive members, the file with the lowest ordinal will be used.

## Ordinals

rld assigns an “ordinal” value to each file. This value is used to select a definition where a choice must be made. Ordinals are critical to ensure that the linker produces consistent output even when threading means that operations occur in an unpredicable order within the linker.


## Shadow Memory

rld uses a block of so-called “shadow memory” to provide O(1) access to symbols. We use atomic operations to access this memory which means that they must be carefully corrdinated across threads.

A shadow pointer may be in any of four states:

1. nullptr. Shadow memory is zero initialized and starts in the “nullptr” state.
2. Busy. The “busy” state is used as a crude synchonization mechanism which can fit into a single shadow-memory pointer. We expect contention to be normally very low, but it ensures that only a single job can update a symbol at any moment.
3. Symbol *. A pointer to a Symbol instance. The symbol may be defined (“def”) or undefined (“undef”).
4. CompilationRef *. A pointer to a CompilationRef instance. This represents the possibility of adding a compilation to the link and will be used to resolve strongly undefined symbols.

Shadow pointer state transitions:

<div><a href='//sketchviz.com/@paulhuggett/589f3a356d51f3d347e41d3ead848e5a'><img src='https://sketchviz.com/@paulhuggett/589f3a356d51f3d347e41d3ead848e5a/58ebb227424a846f62509b3c5390edbfe7c8ceb0.png' style='max-width: 100%;'></a></div>

Shadow memory always starts in the “nullptr” state. The “busy” state is used as a crude synchonization mechanism which can always fit into a single shadow-memory pointer. We expect contention to be normally very low, but it ensures that only a single job can update a pointer at any moment.

Valid transitions:

- nullptr → Busy → Symbol *
- nullptr → Busy → CompilationRef *
- CompilationRef * → Busy → Symbol * (note 2)
- CompilationRef * → Busy → CompilationRef * (note 2)
- Symbol * → Busy → Symbol * (note 1)
- CompilationRef → Symbol * (note 3)

Notes:

1. State changes from an undef-symbol to Busy and back to the same or a different symbol.
2. We can go from a CompilationRef to a Symbol or to a different CompilationRef (with an earlier position).
3. The transition from CompilationRef to Symbol occurs when all of the CompilationRef entries have been “discovered” and a new pass for the front-end is being built. This makes a transition via the Busy state unnecessary.


## Examples

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

    Two things are happening during the processing of Group #0: 
    
    - Symbol resolution for the ticket files presented directly on the command line (f.o in this example).
    - Discovery of symbols that are defined by each of the archives being linked.

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

### Example #2

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

