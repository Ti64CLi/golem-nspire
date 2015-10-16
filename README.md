# Project Golem-lang

A programming language written in C.

# Project aims / long-term goals

- Functional, object-oriented programming language
- Immutablitiy of everything by default
- Explicit mutable declaration
- Statically, strong typed => reliable, eliminates programming errors
- Type inference => automatic deduction of data types
- Strings as arrays of characters, not a standalone type => reduces data size, easier to understand
- Systems language
- Easily readable and learnable, because of familiar syntax
- **NULL** is not an option
- Fully functional fast bytecode vm, comptetitive with lua's or python's speed
- Garbage collected vm
- Golem script (.gs) file extension

# TODO

#### Main tasks (Sorted by importance / Flagged by difficulty)

##### Serious issues

- [ ] (Intermediate) While and if loops don't provide scopes => (Working on it.)

##### Tasks

- [ ] (Easy) Remove unnecessary bytecode operators (FADD, IADD => ADD) => (Working on it.)
- [ ] (Intermediate) More operators for each 'class', to get productive with the code => (Working on it.)
- [ ] (Advanced) Standard libraries and external classes, DLL loading => (Task)
- [ ] (Advanced) Bytecode optimizations => (Working on it.)
- [ ] (Intermediate) Implement computed gotos to gain 15%-20% more speed => (Working on it.)
- [ ] (Easy) Array index out of bounds exceptions => (Task)
- [ ] (Intermediate) Create an try-catch exception system => (Task)
- [ ] (Intermediate) Implement switch statements => (Task)
- [ ] (Intermediate) Do for loops with iterators => (Task)
- [ ] (Intermediate) Do namespaces => (Task)
- [ ] (Advanced) Implement tuples => (Task)

##### Final fixes

- [ ] (Easy) Serious tests / Benchmarks => (Side task)
- [ ] (Intermediate) Improve the REPL => (Long-term-task)

##### Done

- [x] (Intermediate) Serializer for bytecodes => (Done.)
- [x] (Intermediate) Final fixes for arrays (NaN-Tagging) => (Done.)
- [x] (Advanced) Use NaN-Tagging and increase vm performance => (Done.)
- [x] (Intermediate) Create a annotation system => (Done.)
- [x] Move import to compiler side for individual errors => (Done.)
- [x] Import other files for bigger projects => (Done.)
- [x] Class functions allow access to constructor parameters => (Done.)
- [x] Array operators / Allow zero element arrays => (Done.)
- [x] Class field modification within a class function doesn't work as expected => (Done.)
- [x] Comment at end of file is invalid => (Done.)
- [x] Implement new string merge operator => (Done.)
- [x] New operator for reassignment (:=), replace double equal by equal => (Done.)
- [x] Return type isn't checked => (Done.)
- [x] Calling an interal function in a class yields an error => (Done.)
- [x] Classes / Objects parsing + compilation => (Done.)
- [x] Functions with Classes as parameters => (Done.)
- [x] When trying to replace an **Upvalue**, wrong storage address is used => (Done.)
- [x] Nested functions => (Done.)
- [x] Implementing all (basic) operators for associated types in the vm / bytecodes => (Done.)
- [x] Finalize implementing functions => (Done.)
- [x] mark-sweep garbage collector for vm => (Done.)
- [x] Mutable parameters => (Done.)
- [x] Subscripts (Arrays / Strings) => (Done.)

#### Side projects

- [ ] Lambdas, Closures, Anonymous functions => (Concept / Prototype.)
- [ ] Debugger + inspector
- [ ] JIT / ASM output
- [ ] UTF-8 and escape characters

#### Langugage specific

- [ ] AST optimization
- [ ] Lambdas
- [ ] for loops using iterators (only usable for arrays) using pipe syntax, e.g. |x|

# Concept (final concept may change)

### Variables

Each variable is immutable at first. Variables are defined / declared using the 'let' keyword.
Example:
```rust
	let x = 5
```

Variables can be mutable, if they are declared using the 'mut' keyword.
When modifiying a variable, the reassignment operator ':=' is used.
```rust
	let mut x = 5
x := x + 1
```

### Functions

Functions are declared using the 'func' keyword.
You have to declare the parameters and the return type.
Parameters are always immutable, if the 'mut' mutability keyword is not set.
```ruby
	func main(arg0:char[]) -> void
{
	# body
}
```

### Arrays

Arrays are defined using two brackets, in which the content is placed.
An array can only be of one datatype and can not be replaced of modified if it is immutable.
For character arrays, a string initializer can be used.

Integer array:
```rust
	let arr = [1,2,3,4,5]
let str = "Hello World" // Same as ["H", "E", "L", ...]
```

One wide strings / characters

```ruby
	# Declares a character
let c1 = "r"

# Declares a string
let c2 = ["r"]
```

Empty arrays must have a type assigned.
The following form is used: (EBNF)
```sh
	array = '[', '::', type, ']'
```

Example in code:
```ruby
	array = [::int]
```

### Classes

Classes are defined using the structure below.
All attributes are private.
To get a field, getters are used to maintain encapsulation.
Class declaration is also the constructor to maintain immutability.
Class keyword might change.
```objective-c
	using core

type Class(_x:int, _y:int, _z:int) {
	@Getter
	@Setter
	let mut x = _x
	let y = _y
	let z = _z

	func run() -> void {
		println(x)
		println(y)
	}
}

func main() -> void
{
	let cls = Class(7,5,2)
	cls.run()
	println(cls.getX())
}

main()
```

### Control Flow

Control flows are created by if statements or while loops.
Example: (assuming variable 'number' is declared as an integer):
```rust
	if(number = 5)
{
	println("Your number is odd.")
} else if(number = 3)
{
	println("Your number is odd and it's three.")
} else
{
	println("Your number isn't 5 or 3. You should feel bad.")
}
```
While loops:
```ruby
	while(number = 5)
{
	number := number + 1
}
```

For loops:
```ruby
	for(|iter| in [1,2,3,4,5]) {
	print(iter)
}
```

### Annotations

Until now, there are three different types of annotations: @Getter, @Setter and @Unused.
Getter and Setter can only be used within classes and are used to create getter- or setter-methods.
For example we create an attribute named myVar with the value x.

	let myVar = x

If we now want to access myVar outside of the class, the getter annotation is used like this:

	@Getter
	let myVar = x

And the variable is accessed like this:

	let myClass = MyClass(5)
	let res = myClass.getMyVar()

Getter will automatically generate a method returning the value with the name of the variable as prefix.
In the case above get-MyVar-() is used. The same rule applies for the @Setter-annotation.
The first character of the variable name is converted to upper case.

# Syntactic sugar

Subscripts can either be represented by brackets after an expression or by a dot.
Both are internally the same, so class fields can also be accessed by brackets, arrays also by a dot.

Example:

```ruby
	# MyClass x was declared with function getX()
let class = MyClass()

# Normal function call
class.getX()

#Syntactic sugar
class[getX]()
```

# Curently supported

### AST

- variable declaration (immutable/mutable)
- function declaration
- if statements
- while loop
- function calls
- library imports
- array declaration
- subscripts
- expressions with precedence
- classes

### Langugage-based

- variable and function declaration
- types int, char, float, bool, (void), custom classes
- function calls, recursion
- arrays
- system internal functions, e.g. println + getline
- annotations
- relatively fast bytecode vm
- compiler error reports

# Stuff / Internet sources

* [LLVM-C Tutorial](http://markmail.org/download.xqy?id=vhdi5dbzkrdbchxq&number=1)
* [LLVM LangRef](http://llvm.org/docs/LangRef.html)
* [LLVMCCoreValueGeneral](http://llvm.org/docs/doxygen/html/group__LLVMCCoreValueGeneral.html)
* [LLVM Mutable](https://github.com/TheThirdOne/llvm-tutorial-gitbook/blob/master/mutable.md)
* [Ducklang programming language design](http://ducklang.org/designing-a-programming-language-i)
* [A Walk in x86_64 Assembly Land](http://www.codejury.com/a-walk-in-x64-land/)
* [Stack machine](http://www.d.umn.edu/~rmaclin/cs5641/Notes/L19_CodeGenerationI.pdf)
* [Simple scripting language part 5](http://www.incubatorgames.com/20110621/simple-scripting-language-part-5/)
* [Fastest bytecode interpreter (Contest)](http://byteworm.com/2010/11/21/the-fastest-vm-bytecode-interpreter/)
* [Virtual machine (Youtube video)](https://www.youtube.com/watch?v=OjaAToVkoTw)
* [Bob Nystrom's Garbage Collector Explanation](http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/) @munificent
* [Simple virtual machine](http://bartoszsypytkowski.com/simple-virtual-machine/)
* [Functions and stack frames](https://en.wikibooks.org/wiki/X86_Disassembly/Functions_and_Stack_Frames)
* [Lambda lifting](https://en.wikipedia.org/wiki/Lambda_lifting)
* [Call stack](https://en.wikipedia.org/wiki/Call_stack#Structure)
* [Nested function - implementation](https://en.wikipedia.org/wiki/Nested_function#Implementation)
* [Immutable stack](http://amitdev.github.io/coding/2013/12/31/Functional-Stack/)

# Licence
Copyright (c) Alexander Koch 2015 All Rights Reserved.
Project start: 18.05.2015
