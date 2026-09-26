# Unreal Engine Coding Standard

> Write maintainable code by adhering to established standards and best practices.

At Epic Games, we have a few simple coding standards and conventions. This document reflects the
state of Epic Games' current coding standards. **Following the coding standards is mandatory.**

Code conventions are important to programmers for several reasons:

- 80% of the lifetime cost of a piece of software goes to maintenance.
- Hardly any software is maintained for its whole life by the original author.
- Code conventions improve the readability of software, allowing engineers to understand new code
  quickly and thoroughly.
- If we decide to expose source code to mod community developers, we want it to be easily
  understood.
- Many of these conventions are required for cross-compiler compatibility.

The coding standards below are C++-centric; however, the standard is expected to be followed no
matter which language is used. A section may provide equivalent rules or exceptions for specific
languages where it's applicable.

## Table of Contents

- [Class Organization](#class-organization)
- [Copyright Notice](#copyright-notice)
- [Naming Conventions](#naming-conventions)
- [Inclusive Word Choice](#inclusive-word-choice)
  - [Racial, ethnic, and religious inclusiveness](#racial-ethnic-and-religious-inclusiveness)
  - [Gender inclusiveness](#gender-inclusiveness)
  - [Slang](#slang)
  - [Overloaded Words](#overloaded-words)
  - [Word List](#word-list)
- [Portable C++ Code](#portable-c-code)
- [Use of Standard Libraries](#use-of-standard-libraries)
- [Comments](#comments)
  - [Guidelines](#guidelines)
  - [Const Correctness](#const-correctness)
  - [Example Formatting](#example-formatting)
- [Modern C++ Language Syntax](#modern-c-language-syntax)
  - [Static Assert](#static-assert)
  - [Override and Final](#override-and-final)
  - [Nullptr](#nullptr)
  - [Auto](#auto)
  - [Range-Based For](#range-based-for)
  - [Lambdas and Anonymous Functions](#lambdas-and-anonymous-functions)
  - [Strongly-Typed Enums](#strongly-typed-enums)
  - [Move Semantics](#move-semantics)
  - [Default Member Initializers](#default-member-initializers)
- [Third Party Code](#third-party-code)
- [Code Formatting](#code-formatting)
  - [Braces](#braces)
  - [If - Else](#if---else)
  - [Tabs and Indenting](#tabs-and-indenting)
  - [Switch Statements](#switch-statements)
  - [Namespaces](#namespaces)
  - [Physical Dependencies](#physical-dependencies)
  - [Encapsulation](#encapsulation)
  - [General Style Issues](#general-style-issues)
- [API Design Guidelines](#api-design-guidelines)
- [Platform-Specific Code](#platform-specific-code)

---

## Class Organization

Classes should be organized with the reader in mind rather than the writer. Since most readers
will use the public interface of the class, the public implementation should be declared first,
followed by the class's private implementation.

```cpp
UCLASS()
class EXAMPLEPROJECT_API AExampleActor : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AExampleActor();

    // Called every frame
    virtual void Tick(float DeltaTime) override;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
};
```

---

## Copyright Notice

Any source file (`.h`, `.cpp`, `.xaml`) provided by Epic Games for public distribution must
contain a copyright notice as the first line in the file. The format of the notice must
exactly match that shown below:

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.
```

If this line is missing or not formatted properly, CIS will generate an error and fail.

---

## Naming Conventions

When using naming conventions, all code and comments should use U.S. English spelling and grammar.

The first letter of each word in a name (such as type name or variable name) is capitalized.
There is usually no underscore between words. For example, `Health` and `UPrimitiveComponent`
are correct, but `lastMouseCoordinates` or `delta_coordinates` are not.

> This is PascalCase formatting for users who may be familiar with other object oriented
> programming languages.

Type names are prefixed with an additional upper-case letter to distinguish them from variable
names. For example, `FSkin` is a type name, and `Skin` is an instance of type `FSkin`.

**Template classes are prefixed by `T`.**

```cpp
template <typename ObjectType>
class TAttribute
```

**Classes that inherit from `UObject` are prefixed by `U`.**

```cpp
class UActorComponent
```

**Classes that inherit from `AActor` are prefixed by `A`.**

```cpp
class AActor
```

**Classes that inherit from `SWidget` are prefixed by `S`.**

```cpp
class SCompoundWidget
```

**Classes that are abstract interfaces are prefixed by `I`.**

```cpp
class IAnalyticsProvider
```

**Epic's concept-alike struct types are prefixed by `C`.**

```cpp
struct CStaticClassProvider
{
    template <typename T>
    auto Requires(UClass*& ClassRef) -> decltype(
        ClassRef = T::StaticClass()
    );
};
```

**Enums are prefixed by `E`.**

```cpp
enum class EColorBits
{
    ECB_Red,
    ECB_Green,
    ECB_Blue
};
```

**Boolean variables must be prefixed by `b`.**

```cpp
bPendingDestruction
bHasFadedIn
```

Most other classes are prefixed by `F`, though some subsystems use other letters.

Typedefs should be prefixed by whatever is appropriate for that type, such as:

- `F` for typedef of a struct
- `U` for typedef of a `UObject`

A typedef of a particular template instantiation is no longer a template and should be
prefixed accordingly.

```cpp
typedef TArray<FMytype> FArrayOfMyTypes;
```

Prefixes are omitted in C#.

Unreal Header Tool requires the correct prefixes in most cases, so it's important to provide them.

Type template parameters and nested type aliases based on those template parameters are not
subject to the above prefix rules, as the type category is unknown.

- Prefer a `Type` suffix after a descriptive term.
- Disambiguate template parameters from aliases by using an `In` prefix:

```cpp
template <typename InElementType>
class TContainer
{
public:
    using ElementType = InElementType;
};
```

- Type and variable names are nouns.
- Method names are verbs that either describe the method's effect, or the return value of a
  method without an effect.
- Macro names should be fully capitalized with words separated by underscores, and prefixed
  with `UE_`.

```cpp
#define UE_AUDIT_SPRITER_IMPORT
```

Variable, method, and class names should be:

- Clear
- Unambiguous
- Descriptive

The greater the scope of the name, the greater the importance of a good, descriptive name.
Avoid over-abbreviation.

All variables should be declared on their own line so that you can provide comment on the
meaning of each variable.

- The JavaDocs style requires it.
- You can use multi-line or single-line comments before a variable. Blank lines are optional
  for grouping variables.

All functions that return a `bool` should ask a true/false question, such as `IsVisible()` or
`ShouldClearBuffer()`.

A procedure (a function with no return value) should use a strong verb followed by an Object.
An exception is, if the Object of the method is the Object it is in. In this case, the Object
is understood from context. Names to avoid include those beginning with "Handle" and "Process"
because the verbs are ambiguous.

We encourage you to prefix function parameter names with "Out" if:

- The function parameters are passed by reference.
- The function is expected to write to that value.

This makes it obvious that the value passed in this argument is replaced by the function.

If an `In` or `Out` parameter is also a boolean, put `b` before the `In`/`Out` prefix, such as
`bOutResult`.

Functions that return a value should describe the return value. The name should make clear
what value the function returns. This is particularly important for boolean functions.
Consider the following two example methods:

```cpp
// what does true mean?
bool CheckTea(FTea Tea);

// name makes it clear true means tea is fresh
bool IsTeaFresh(FTea Tea);

float TeaWeight;
int32 TeaCount;
bool bDoesTeaStink;
FName TeaName;
```

---

## Inclusive Word Choice

When you work in the Unreal Engine codebase, we encourage you to strive to use respectful,
inclusive, and professional language.

Word choice applies when you name:

- classes
- functions
- data structures
- types
- variables
- files and folders
- plugins

It applies when you write snippets of user-facing text for the UI, error messages, and
notifications. It also applies when writing about code, such as in comments and changelist
descriptions.

The following sections provide guidance and suggestions to help you choose words and names
that are respectful and appropriate for all situations and audiences, and be a more effective
communicator.

### Racial, ethnic, and religious inclusiveness

- Do not use metaphors or similes that reinforce stereotypes. Examples include _contrast black
  and white_ or _blacklist and whitelist_.
- Do not use words that refer to historical trauma or lived experience of discrimination.
  Examples include _slave_, _master_, and _nuke_.

### Gender inclusiveness

- Refer to hypothetical people as _they_, _them_, and _their_, even in the singular.
- Refer to anything that is not a person as _it_ and _its_. For example, a module, plugin,
  function, client, server, or any other software or hardware component.
- Do not assign a gender to anything that doesn't have one.
- Do not use collective nouns like _guys_ that assume gender.
- Avoid colloquial phrases that contain arbitrary genders, like "a poor man's X".

### Slang

Remember that your words are being read by a global audience that may not share the same
idioms and attitudes, and who might not understand the same cultural references.

- Avoid slang and colloquialisms, even if you think they are funny or harmless. These may be
  hard to understand for people whose first language is not English, and might not translate
  well.
- Do not use profanity.

### Overloaded Words

Many terms that we use for their technical meanings also have other meanings outside of
technology. Examples include _abort_, _execute_, or _native_. When you use words like these,
always be precise and examine the context in which they appear.

### Word List

The following list identifies some terminology that we have used in the Unreal Engine codebase
in the past, but that we believe should be replaced with better alternatives:

| Word Name | Alternative Word Name                                                                             |
| --------- | ------------------------------------------------------------------------------------------------- |
| Blacklist | deny list, block list, exclude list, avoid list, unapproved list, forbidden list, permission list |
| Whitelist | allow list, include list, trust list, safe list, prefer list, approved list, permission list      |
| Master    | primary, source, controller, template, reference, main, leader, original, base                    |
| Slave     | secondary, replica, agent, follower, worker, cluster node, locked, linked, synchronized           |

We are actively working to bring our code in line with the principles laid out above.

---

## Portable C++ Code

The `int` and `unsigned int` types vary in size across platforms. They are guaranteed to be at
least 32 bits in width and are acceptable in code when the integer width is unimportant.
Explicitly-sized types are used in serialized or replicated formats.

Below is a list of common types:

| Type     | Description                                                                |
| -------- | -------------------------------------------------------------------------- |
| `bool`   | Boolean values (NEVER assume the size of `bool`). `BOOL` will not compile. |
| `TCHAR`  | A character (NEVER assume the size of `TCHAR`).                            |
| `uint8`  | Unsigned bytes (1 byte).                                                   |
| `int8`   | Signed bytes (1 byte).                                                     |
| `uint16` | Unsigned shorts (2 bytes).                                                 |
| `int16`  | Signed shorts (2 bytes).                                                   |
| `uint32` | Unsigned ints (4 bytes).                                                   |
| `int32`  | Signed ints (4 bytes).                                                     |
| `uint64` | Unsigned quad words (8 bytes).                                             |
| `int64`  | Signed quad words (8 bytes).                                               |
| `float`  | Single precision floating point (4 bytes).                                 |
| `double` | Double precision floating point (8 bytes).                                 |
| `PTRINT` | An integer that may hold a pointer (NEVER assume the size of `PTRINT`).    |

---

## Use of Standard Libraries

Historically, UE has avoided direct use of the C and C++ standard libraries for the following
reasons:

- Replace slow implementations with our own that provide additional control over memory
  allocation.
- Add new functionality before it's widely available, such as:
  - Making desirable, but non-standard, behavioral changes.
  - Having consistent syntax across the codebase.
  - Avoiding constructs which are incompatible with UE's idioms.

However, the standard library has matured and includes functionality that we don't want to wrap
with an abstraction layer or reimplement ourselves.

When there is a choice between a standard library feature instead of our own, you should prefer
the option that gives superior results. It's also important to remember that consistency is
valued. If a legacy UE implementation is no longer serving a purpose, we may choose to
deprecate it and migrate all usage toward the standard library.

Avoid mixing UE idioms and standard library idioms in the same API. The following table lists
common idioms along with recommendations on when to use them.

| Idiom                                  | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| -------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `<atomic>`                             | The atomic idiom should be used in new code and old migrated when touched. Atomics are expected to be implemented fully and efficiently on all supported platforms. Our own `TAtomic` is only partially implemented, and it isn't in our interest to maintain and improve it.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `<type_traits>`                        | The type traits idiom should be used where there's overlap between a legacy UE trait and a standard trait. Traits are often implemented as compiler intrinsics for correctness, and compilers can have knowledge of the standard traits and select faster compilation paths instead of treating them as plain C++.<br><br>One concern is that our traits typically have an upper case `Value` static or `Type` typedef, whereas standard traits are expected to use `value` and `type`. This is an important distinction, as a particular syntax is expected by compositional traits, for example `std::conjunction`.<br><br>New traits we add should be written with lowercase `value` or `type` to support composition. Existing traits should be updated to support either case. |
| `<initializer_list>`                   | The initializer list idiom must be used to support braced initializer syntax. This is a case where the language and the standard libraries overlap. There is no alternative if you want to support it.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| `<regex>`                              | The regex idiom may be used directly, but its use should be encapsulated within editor-only code. We have no plans to implement our own regex solution.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| `<limits>`                             | `std::numeric_limits` can be used in its entirety.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| `<cmath>`                              | All floating point functions from this header may be used.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| `<cstring>`: `memcpy()` and `memset()` | These idioms can be used instead of `FMemory::Memcpy` and `FMemory::Memset` respectively, when they have a demonstrable performance benefit.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        |

Standard containers and strings should be avoided except in interop code.

---

## Comments

Comments are communication and communication is vital. The following sections detail some
important things to keep in mind about comments (from Kernighan & Pike _The Practice of
Programming_).

### Guidelines

**Write self-documenting code.** For example:

```cpp
// Bad:
t = s + l - b;

// Good:
TotalLeaves = SmallLeaves + LargeLeaves - SmallAndLargeLeaves;
```

**Write useful comments.** For example:

```cpp
// Bad:
// increment Leaves
++Leaves;

// Good:
// we know there is another tea leaf
++Leaves;
```

**Do not over comment bad code — rewrite it instead.** For example:

```cpp
// Bad:
// total number of leaves is sum of
// small and large leaves less the
// number of leaves that are both
t = s + l - b;

// Good:
TotalLeaves = SmallLeaves + LargeLeaves - SmallAndLargeLeaves;
```

**Do not contradict the code.** For example:

```cpp
// Bad:
// never increment Leaves!
++Leaves;

// Good:
// we know there is another tea leaf
++Leaves;
```

### Const Correctness

Const is documentation as much as it is a compiler directive. All code should strive to be
const-correct. This includes the following guidelines:

- Pass function arguments by const pointer or reference if those arguments are not intended to
  be modified by the function.
- Flag methods as const if they do not modify the object.
- Use const iteration over containers if the loop isn't intended to modify the container.

Example:

```cpp
void SomeMutatingOperation(FThing& OutResult, const TArray<Int32>& InArray)
{
    // InArray will not be modified here, but OutResult probably will be
}

void FThing::SomeNonMutatingOperation() const
{
    // This code will not modify the FThing it is invoked on
}
```

Const is also preferred for by-value function parameters and locals. This tells the reader that
the variable will not be modified in the body of the function, which makes it easier to
understand. If you do this, make sure that the declaration and the definition match, as this
can affect the JavaDoc process.

```cpp
void AddSomeThings(const int32 Count);

void AddSomeThings(const int32 Count)
{
    const int32 CountPlusOne = Count + 1;
    // Neither Count nor CountPlusOne can be changed during the body of the function
}
```

One exception to this is pass-by-value parameters, which are moved into a container. For more
information, see the [Move Semantics](#move-semantics) section on this page.

Example:

```cpp
void FBlah::SetMemberArray(TArray<FString> InNewArray)
{
    MemberArray = MoveTemp(InNewArray);
}
```

Put the `const` keyword on the end when making a pointer itself const (rather than what it
points to). References can't be "reassigned" anyway, and so can't be made const in the same way.

Example:

```cpp
// Const pointer to non-const object - pointer cannot be reassigned, but T can still be modified
T* const Ptr = ...;

// Illegal
T& const Ref = ...;
```

Never use const on a return type. This inhibits move semantics for complex types, and will give
compile warnings for built-in types. This rule only applies to the return type itself, not the
target type of a pointer or reference being returned.

Example:

```cpp
// Bad - returning a const array
const TArray<FString> GetSomeArray();

// Fine - returning a reference to a const array
const TArray<FString>& GetSomeArray();

// Fine - returning a pointer to a const array
const TArray<FString>* GetSomeArray();

// Bad - returning a const pointer to a const array
const TArray<FString>* const GetSomeArray();
```

### Example Formatting

We use a system based on JavaDoc to extract comments from the code and build documentation
automatically, therefore we recommend specific comment formatting rules.

The following example demonstrates the format of class, method, and variable comments. Remember
that comments should augment the code. Code documents the implementation while comments
document the intent. Make sure to update comments when you change the intent of a piece of code.

Note that two different parameter comment styles are supported, shown by the `Steep` and
`Sweeten` methods. The `@param` style used by `Steep` is the traditional multi-line style. For
simple functions, it can be clearer to integrate the parameter and return value documentation
into the descriptive comment for the function. This is demonstrated in the `Sweeten` example.
Special comment tags like `@see` or `@return` should only be used to start new lines following
the primary description.

Method comments should only be included once: where the method is publicly declared. The method
comments should only contain information relevant to callers of the method, including any
information about overrides of the method that may be relevant to the caller. Details about the
implementation of the method and its overrides, that are not relevant to callers, should be
commented within the method implementation.

**Class comments should include:**

- A description of the problem this class solves.
- The reason why this class was created.

**Multi-line method comments should include:**

- **Function purpose:** Documents the problem this function solves. As previously stated,
  comments document intent, and code documents implementation.
- **Parameter comments:** Each parameter comment should include:
  - units of measure;
  - the range of expected values;
  - "impossible" values;
  - and the meaning of status/error codes.
- **Return comment:** Documents the expected return value, just as an output variable is
  documented. To avoid redundancy, an explicit `@return` comment should not be used if the sole
  purpose of the function is to return this value and that is already documented in the
  function purpose.
- **Extra information:** `@warning`, `@note`, `@see`, and `@deprecated` can optionally be used
  to document additional relevant information. Each should be declared on their own line
  following the rest of the comments.

---

## Modern C++ Language Syntax

Unreal Engine is built to be massively portable to many C++ compilers, so we are careful to use
features that are compatible with the compilers we might be supporting. Sometimes, features are
so useful that we will wrap them in macros and use them pervasively. However, we usually wait
until all the compilers we support are up to the latest standard.

Unreal Engine compiles with a language version of C++20 by default and requires a minimum
version of C++20 to build. We use many modern language features that are well-supported across
modern compilers. In some cases, we wrap usage of these features in preprocessor conditionals.
However, sometimes we decide to avoid certain language features entirely, for portability or
other reasons.

Unless specified below, as a modern C++ compiler feature we are supporting, you should not use
compiler-specific language features unless they are wrapped in preprocessor macros or
conditionals and used sparingly.

### Static Assert

The `static_assert` keyword is valid for use where you need a compile-time assertion.

### Override and Final

The `override` and `final` keywords are valid for use, and their use is strongly encouraged.
There might be many places where these have been omitted, but they will be fixed over time.

### Nullptr

You should use `nullptr` instead of the C-style `NULL` macro in all cases.

One exception to this is the use of `nullptr` in C++/CX builds (such as for Xbox One). In this
case, the use of `nullptr` is actually the managed null reference type. It is mostly compatible
with `nullptr` from native C++ except in its type and some template instantiation contexts, and
so you should use the `TYPE_OF_NULLPTR` macro instead of the more usual `decltype(nullptr)` for
compatibility.

### Auto

You shouldn't use `auto` in C++ code, except for the few exceptions listed below. Always be
explicit about the type you're initializing. This means that the type must be plainly visible
to the reader. This rule also applies to the use of the `var` keyword in C#.

C++20's structured binding feature should also not be used, as it is effectively a variadic
`auto`.

Acceptable use of `auto`:

- When you need to bind a lambda to a variable, as lambda types are not expressible in code.
- For iterator variables, but only where the iterator's type is verbose and would impair
  readability.
- In template code, where the type of an expression cannot easily be discerned. This is an
  advanced case.

It's very important that types are clearly visible to someone who is reading the code. Even
though some IDEs are able to infer the type, doing so relies on the code being in a compilable
state. It also won't assist users of merge/diff tools, or when viewing individual source files
in isolation, such as on GitHub.

If you're sure you are using `auto` in an acceptable way, always remember to correctly use
`const`, `&`, or `*` just like you would with the type name. With `auto`, this will coerce the
inferred type to be what you want.

### Range-Based For

This is preferred to keep the code easier to understand and more maintainable. When you migrate
code that uses old `TMap` iterators, be aware that the old `Key()` and `Value()` functions,
which were methods of the iterator type, are now simply `Key` and `Value` fields of the
underlying key-value `TPair`.

Example:

```cpp
TMap<FString, int32> MyMap;

// Old style
for (auto It = MyMap.CreateIterator(); It; ++It)
{
    UE_LOG(LogCategory, Log, TEXT("Key: %s, Value: %d"), It.Key(), *It.Value());
}

// New style
for (TPair<FString, int32>& Kvp : MyMap)
{
    UE_LOG(LogCategory, Log, TEXT("Key: %s, Value: %d"), *Kvp.Key, Kvp.Value);
}
```

We also have range replacements for some standalone iterator types.

Example:

```cpp
// Old style
for (TFieldIterator<UProperty> PropertyIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
{
    UProperty* Property = *PropertyIt;
    UE_LOG(LogCategory, Log, TEXT("Property name: %s"), *Property->GetName());
}

// New style
for (UProperty* Property : TFieldRange<UProperty>(InStruct, EFieldIteratorFlags::IncludeSuper))
{
    UE_LOG(LogCategory, Log, TEXT("Property name: %s"), *Property->GetName());
}
```

### Lambdas and Anonymous Functions

Lambdas can be used freely but come with additional safety concerns. The best lambdas should be
no more than a couple of statements in length, particularly when used as part of a larger
expression or statement, for example as a predicate in a generic algorithm.

Example:

```cpp
// Find first Thing whose name contains the word "Hello"
Thing* HelloThing = ArrayOfThings.FindByPredicate([](const Thing& Th){ return Th.GetName().Contains(TEXT("Hello")); });

// Sort array in reverse order of name
Algo::Sort(ArrayOfThings, [](const Thing& Lhs, const Thing& Rhs){ return Lhs.GetName() > Rhs.GetName(); });
```

Be aware that stateful lambdas can't be assigned to function pointers, which we tend to use a
lot. Non-trivial lambdas should be documented in the same manner as regular functions. Lambdas
can also be used as Delegates for deferred execution using functions like `BindWeakLambda`
where captured variables function as a payload.

#### Captures and Return Types

Explicit captures should be used rather than automatic capture (`[&]` and `[=]`). This is
important for readability, maintainability, safety, and performance reasons, particularly when
used with large lambdas and deferred execution.

Explicit captures declare the author's intent; therefore, mistakes are caught during code
review. Incorrect captures can cause serious bugs and crashes, which are more likely to become
problematic as the code is maintained over time. Here are some additional things to keep in
mind about lambda captures:

- By-reference capture and by-value capture of pointers (including the `this` pointer) can
  cause data corruption and crashes if the execution of the lambda is deferred. Local and
  member variables should never be captured by reference for deferred lambdas.
- By-value capture can be a performance concern if it makes unnecessary copies for a
  non-deferred lambda.
- Accidentally captured `UObject` pointers are invisible to the garbage collector. Automatic
  capture catches this implicitly if any member variables are referenced, even though `[=]`
  gives the impression of the lambda having its own copies of everything.
- Delegate wrappers like `CreateWeakLambda` and `CreateSPLambda` should be used for deferred
  execution as they will automatically unbind if the `UObject` or shared pointer are freed.
  Other shared objects can be captured as `TWeakObjectPtr` or `TWeakPtr` and then validated
  inside the lambda.

Any deferred lambda use that does not follow these guidelines must have a comment explaining
why the lambda capture is safe.

Explicit return types should be used for large lambdas or when you are returning the result of
another function call. These should be considered in the same way as the `auto` keyword.

### Strongly-Typed Enums

Enumerated (Enum) classes are a replacement for old-style namespaced enums, both for regular
enums and `UENUM`s. For example:

```cpp
// Old enum
UENUM()
namespace EThing
{
    enum Type
    {
        Thing1,
        Thing2
    };
}

// New enum
UENUM()
enum class EThing : uint8
{
    Thing1,
    Thing2
};
```

Enums are supported as `UPROPERTY`s, and replace the old `TEnumAsByte<>` workaround. Enum
properties can also be any size, not just bytes:

```cpp
// Old property
UPROPERTY()
TEnumAsByte<EThing::Type> MyProperty;

// New property
UPROPERTY()
EThing MyProperty;
```

Enums exposed to Blueprints must continue to be based on `uint8`.

Enum classes used as flags can take advantage of the `ENUM_CLASS_FLAGS(EnumType)` macro to
automatically define all of the bitwise operators:

```cpp
enum class EFlags
{
    None  = 0x00,
    Flag1 = 0x01,
    Flag2 = 0x02,
    Flag3 = 0x04
};

ENUM_CLASS_FLAGS(EFlags)
```

The one exception to this is the use of flags in a truth context — this is a limitation of the
language. Instead, all enum flags should have an enumerator called `None` which is set to `0`
for comparisons:

```cpp
// Old
if (Flags & EFlags::Flag1)

// New
if ((Flags & EFlags::Flag1) != EFlags::None)
```

### Move Semantics

All of the main container types — `TArray`, `TMap`, `TSet`, `FString` — have move constructors
and move assignment operators. These are often used automatically when passing or returning
these types by value. They can also be explicitly invoked by using `MoveTemp`, UE's equivalent
of `std::move`.

Returning containers or strings by value can be beneficial for expressivity, without the usual
cost of temporary copies. Rules around pass-by-value and use of `MoveTemp` are still being
established, but can already be found in some optimized areas of the codebase.

### Default Member Initializers

Default member initializers can be used to define the defaults of a class inside the class
itself:

```cpp
UCLASS()
class UTeaOptions : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    int32 MaximumNumberOfCupsPerDay = 10;

    UPROPERTY()
    float CupSizeInMilliliters = 200.0f;
};
```

Code written like this has the following benefits:

- It doesn't need to duplicate initializers across multiple constructors.
- It isn't possible to mix the initialization order and declaration order.
- The member type, property flags, and default values are all in one place. This helps
  readability and maintainability.

However, there are also some downsides:

- Any change to the defaults requires a rebuild of all dependent files.
- Headers can't change in patch releases of the engine, so this style can limit the kinds of
  fixes that are possible.
- Some things can't be initialized in this way, such as base classes, `UObject` subobjects,
  pointers to forward-declared types, values deduced from constructor arguments, and members
  initialized over multiple steps.
- Putting some initializers in the header and the rest in constructors in the `.cpp` file can
  reduce readability and maintainability.

Use your best judgment when deciding whether to use default member initializers. As a rule of
thumb, default member initializers make more sense with in-game code than engine code. Consider
using config files for default values.

---

## Third Party Code

Whenever you modify the code to a library that we use in the engine, be sure to tag your
changes with a `//@UE5` comment, as well as an explanation of why you made the change. This
makes merging the changes into a new version of that library easier, and ensures licensees can
easily find any modifications we have made.

Any third party code included in the engine should be marked with comments formatted to be
easily searchable. For example:

```cpp
// @third party code - BEGIN PhysX
#include <physx.h>
// @third party code - END PhysX

// @third party code - BEGIN MSDN SetThreadName
// [http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx]
// Used to set the thread name in the debugger
...
//@third party code - END MSDN SetThreadName
```

---

## Code Formatting

### Braces

**Avoid brace conflicts.** Epic Games has a long standing usage pattern of putting braces on a
new line. Please adhere to this usage, regardless of the size of the function or block. For
example:

```cpp
// Bad
int32 GetSize() const { return Size; }

// Good
int32 GetSize() const
{
    return Size;
}
```

**Always include braces in single-statement blocks.** For example:

```cpp
if (bThing)
{
    return;
}
```

### If - Else

Each block of execution in an if-else statement should be in braces. This helps prevent editing
mistakes. When braces are not used, someone could unwittingly add another line to an `if` block.
The extra line wouldn't be controlled by the `if` expression, which would be bad. It's also bad
when conditionally compiled items cause if/else statements to break. So always use braces.

```cpp
if (bHaveUnrealLicense)
{
    InsertYourGameHere();
}
else
{
    CallMarkRein();
}
```

A multi-way if statement should be indented with each `else if` indented the same amount as the
first `if`; this makes the structure clear to a reader:

```cpp
if (TannicAcid < 10)
{
    UE_LOG(LogCategory, Log, TEXT("Low Acid"));
}
else if (TannicAcid < 100)
{
    UE_LOG(LogCategory, Log, TEXT("Medium Acid"));
}
else
{
    UE_LOG(LogCategory, Log, TEXT("High Acid"));
}
```

### Tabs and Indenting

Below are some standards for indenting your code.

- Indent code by execution block.
- Use tabs for whitespace at the beginning of a line, not spaces. Set your tab size to 4
  characters. Note, spaces are sometimes necessary and allowed for keeping code aligned
  regardless of the number of spaces in a tab. For example, when you are aligning code that
  follows non-tab characters.
- If you are writing code in C#, please also use tabs, not spaces. The reason for this is that
  programmers often switch between C# and C++, and most prefer to use a consistent setting for
  tabs. Visual Studio defaults to using spaces for C# files, so you need to remember to change
  this setting when working on Unreal Engine code.

### Switch Statements

Except for empty cases (multiple cases having identical code), switch case statements should
explicitly label that a case falls through to the next case. Either include a `break`, or
include a "falls through" comment in each case. Other code control-transfer commands (`return`,
`continue`, and so on) are fine as well.

Always have a `default` case. Include a `break` just in case someone adds a new case after the
default.

```cpp
switch (condition)
{
    case 1:
        ...
        // falls through

    case 2:
        ...
        break;

    default:
        break;
}
```

### Namespaces

You can use namespaces to organize your classes, functions and variables where appropriate. If
you do use them, follow the rules below.

- Most UE code is currently not wrapped in a global namespace.
- Be careful to avoid collisions in the global scope, especially when using or including third
  party code.
- Namespaces are not supported by Unreal Header Tool.
  - Namespaces should not be used when defining `UCLASS`es, `USTRUCT`s and so on.
- New APIs which aren't `UCLASS`es, `USTRUCT`s etc, should be placed in a `UE::` namespace, and
  ideally a nested namespace, e.g. `UE::Audio::`.
- Namespaces which are used to hold implementation details which are not part of the
  public-facing API should go in a `Private` namespace, e.g. `UE::Audio::Private::`.

**Using declarations:**

- Do not put `using` declarations in the global scope, even in a `.cpp` file (it will cause
  problems with our "unity" build system).
- It's okay to put `using` declarations within another namespace, or within a function body.
  - If you put `using` declarations within a namespace, this will carry over to other
    occurrences of that namespace in the same translation unit. As long as you are consistent,
    it will be fine.
  - You can only use `using` declarations in header files safely if you follow the above rules.
- Forward-declared types need to be declared within their respective namespace.
  - If you don't do this, you will get link errors.
  - If you declare a lot of classes or types within a namespace, it can be difficult to use
    those types in other global-scoped classes (for example, function signatures will need to
    use explicit namespace when appearing in class declarations).
- You can use `using` declarations to only alias specific variables within a namespace into
  your scope.
  - For example, `using Foo::FBar`. However, we don't usually do that in Unreal Engine code.
- Macros cannot live in a namespace.
  - They should be prefixed with `UE_` instead of living in a namespace, for example `UE_LOG`.

### Physical Dependencies

**File names should not be prefixed where possible.**

For example, `Scene.cpp` instead of `UScene.cpp`. This makes it easy to use tools like Workspace
Whiz or Visual Assist's Open File in Solution, by reducing the number of letters needed to
identify the file you want.

**All headers should protect against multiple includes with the `#pragma once` directive.**

Note that all compilers we use support `#pragma once`.

```cpp
#pragma once
//<file contents>
```

**Try to minimize physical coupling.**

- In particular, avoid including standard library headers from other headers.
- Forward declarations are preferred to including headers.

**When including a header, be as fine grained as possible.**

For example, do not include `Core.h`. Instead, you should include the specific headers in Core
that you need definitions from.

**Try to include every header you need directly to make fine-grained inclusion easier.**

- Don't rely on a header that is included indirectly by another header you include.
- Don't rely on anything being included through another header. Include everything you need.

**Modules have `Private` and `Public` source directories.**

Any definitions that are needed by other modules must be in headers in the `Public` directory.
Everything else should be in the `Private` directory. In older Unreal modules, these directories
may still be called "Src" and "Inc", but those directories are meant to separate private and
public code in the same way, and are not meant to separate header files from source files.

**Don't worry about setting up your headers for precompiled header generation.**

Unreal Build Tool can do a better job of this than you can.

**Split large functions into logical sub-functions.**

One area of compilers' optimizations is the elimination of common subexpressions. The larger
your functions are, the more work the compiler has to do to identify them. This leads to greatly
inflated build times.

**Don't use a large number of inline functions.**

Inline functions force rebuilds even in files which don't use them. Inline functions should only
be used for trivial accessors and when profiling shows there is a benefit to doing so.

**Be conservative in your use of `FORCEINLINE`.**

All code and local variables will be expanded out into the calling function. This will cause the
same build time problems as those caused by large functions.

### Encapsulation

Enforce encapsulation with the protection keywords. Class members should almost always be
declared private unless they are part of the public/protected interface to the class. Use your
best judgment, but always be aware that a lack of accessors makes it hard to refactor later
without breaking plugins and existing projects.

- If particular fields are only intended to be usable by derived classes, make them private and
  provide protected accessors.
- Use `final` if your class is not designed to be derived from.

### General Style Issues

**Minimize dependency distance.**

When code depends on a variable having a certain value, try to set that variable's value right
before using it. Initializing a variable at the top of an execution block, and not using it for
a hundred lines of code, gives lots of space for someone to accidentally change the value
without realizing the dependency. Having it on the next line makes it clear why the variable is
initialized the way it is and where it is used.

**Split methods into sub-methods where possible.**

It is easier for someone to look at a big picture, and then drill down to the interesting
details, than it is to start with the details and reconstruct the big picture from them. In the
same way, it is easier to understand a simple method, that calls a sequence of several
well-named sub-methods, than it is to understand an equivalent method that simply contains all
the code in those sub-methods.

**In function declarations or function call sites, do not add a space between the function's
name and the parentheses that precede the argument list.**

**Address compiler warnings.**

Compiler warning messages mean something is wrong. Fix what the compiler is warning you about.
If you absolutely can't address it, use `#pragma` to suppress the warning, but this should only
be done as a last resort.

**Leave a blank line at the end of the file.**

All `.cpp` and `.h` files should include a blank line, to coordinate with gcc.

**Debug code should either be useful and polished, or not checked in.**

Debug code that is intermixed with other code makes the other code harder to read.

**Always use the `TEXT()` macro around string literals.**

Without the `TEXT()` macro, code that constructs `FString`s from literals will cause an
undesirable string conversion process.

**Avoid repeating the same operation redundantly in loops.**

Move common subexpressions out of loops to avoid redundant calculations. Make use of statics in
some cases, to avoid globally-redundant operations across function calls, such as constructing
an `FName` from a string literal.

**Be mindful of hot reload.**

Minimize dependencies to cut down on iteration time. Don't use inlining or templates for
functions which are likely to change over a reload. Only use statics for things which are
expected to remain constant over a reload.

**Use intermediate variables to simplify complicated expressions.**

If you have a complicated expression, it can be easier to understand if you split it into
sub-expressions, that are assigned to intermediate variables, with names describing the meaning
of the sub-expression within the parent expression. For example:

```cpp
if ((Blah->BlahP->WindowExists->Etc && Stuff) &&
    !(bPlayerExists && bGameStarted && bPlayerStillHasPawn &&
    IsTuesday())))
{
    DoSomething();
}
```

Should be replaced with:

```cpp
const bool bIsLegalWindow = Blah->BlahP->WindowExists->Etc && Stuff;
const bool bIsPlayerDead = bPlayerExists && bGameStarted && bPlayerStillHasPawn && IsTuesday();
if (bIsLegalWindow && !bIsPlayerDead)
{
    DoSomething();
}
```

**Pointers and references should only have one space to the right of the pointer or reference.**

This makes it easy to quickly use Find in Files for all pointers or references to a certain
type. For example:

```cpp
// Use this
FShaderType* Ptr

// Do not use these:
FShaderType *Ptr
FShaderType * Ptr
```

**Shadowed variables are not allowed.**

C++ allows variables to be shadowed from an outer scope, but this makes usage ambiguous to a
reader. For example, there are three usable `Count` variables in this member function:

```cpp
class FSomeClass
{
public:
    void Func(const int32 Count)
    {
        for (int32 Count = 0; Count != 10; ++Count)
        {
            // Use Count
        }
    }

private:
    int32 Count;
};
```

**Avoid using anonymous literals in function calls.**

Prefer named constants which describe their meaning. This makes intent more obvious to a casual
reader as it avoids the need to look up the function declaration to understand it.

```cpp
// Old style
Trigger(TEXT("Soldier"), 5, true);

// New style
const FName ObjectName                = TEXT("Soldier");
const float CooldownInSeconds         = 5;
const bool bVulnerableDuringCooldown  = true;
Trigger(ObjectName, CooldownInSeconds, bVulnerableDuringCooldown);
```

**Avoid defining non-trivial static variables in headers.**

Non-trivial static variables cause an instance to be compiled into every translation unit that
includes that header:

```cpp
// SomeModule.h
static const FString GUsefulNamedString = TEXT("String");
```

Replace the above with:

```cpp
// SomeModule.h
extern SOMEMODULE_API const FString GUsefulNamedString;

// SomeModule.cpp
const FString GUsefulNamedString = TEXT("String");
```

**Avoid making extensive changes which do not change the code's behavior** (for example:
changing whitespace or mass renaming of private variables) as these cause unnecessary noise in
source history and are disruptive when merging.

- If such a change is important, for example fixing broken indentation caused by an automated
  merge tool, it should be submitted on its own and not mixed with behavioral changes.
- Prefer to fix whitespace or other minor coding standard violations only when other edits are
  being made to the same lines or nearby code.

---

## API Design Guidelines

**Boolean function parameters should be avoided.**

In particular, boolean parameters should be avoided for flags passed to functions. These have
the same anonymous literal problem as mentioned previously, but they also tend to multiply over
time as APIs get extended with more behavior. Instead, prefer an enum (see the advice on use of
enums as flags in the [Strongly-Typed Enums](#strongly-typed-enums) section):

```cpp
// Old style
FCup* MakeCupOfTea(FTea* Tea, bool bAddSugar = false, bool bAddMilk = false, bool bAddHoney = false, bool bAddLemon = false);
FCup* Cup = MakeCupOfTea(Tea, false, true, true);

// New style
enum class ETeaFlags
{
    None,
    Milk  = 0x01,
    Sugar = 0x02,
    Honey = 0x04,
    Lemon = 0x08
};
ENUM_CLASS_FLAGS(ETeaFlags)

FCup* MakeCupOfTea(FTea* Tea, ETeaFlags Flags = ETeaFlags::None);
FCup* Cup = MakeCupOfTea(Tea, ETeaFlags::Milk | ETeaFlags::Honey);
```

This form prevents the accidental transposing of flags, avoids accidental conversion from
pointer and integer arguments, removes the need to repeat redundant defaults, and is more
efficient.

It is acceptable to use bools as arguments when they are the complete state to be passed to a
function like a setter, such as `void FWidget::SetEnabled(bool bEnabled)`. Though consider
refactoring if this changes.

**Avoid overly-long function parameter lists.**

If a function takes many parameters then consider passing a dedicated struct instead:

```cpp
// Old style
TUniquePtr<FCup[]> MakeTeaForParty(const FTeaFlags* TeaPreferences, uint32 NumCupsToMake, FKettle* Kettle, ETeaType TeaType = ETeaType::EnglishBreakfast, float BrewingTimeInSeconds = 120.0f);

// New style
struct FTeaPartyParams
{
    const FTeaFlags* TeaPreferences       = nullptr;
    uint32           NumCupsToMake        = 0;
    FKettle*         Kettle               = nullptr;
    ETeaType         TeaType              = ETeaType::EnglishBreakfast;
    float            BrewingTimeInSeconds = 120.0f;
};
TUniquePtr<FCup[]> MakeTeaForParty(const FTeaPartyParams& Params);
```

**Avoid overloading functions by `bool` and `FString`.**

This can have unexpected behavior:

```cpp
void Func(const FString& String);
void Func(bool bBool);

Func(TEXT("String")); // Calls the bool overload!
```

**Interface classes should always be abstract.**

Interface classes are prefixed with "I" and must not have member variables. Interfaces are
allowed to contain methods that are not pure-virtual, and can contain methods that are
non-virtual or static, as long as they are implemented inline.

**Use the `virtual` and `override` keywords when declaring an overriding method.**

When declaring a virtual function in a derived class that overrides a virtual function in the
parent class, you must use both the `virtual` and the `override` keywords. For example:

```cpp
class A
{
public:
    virtual void F() {}
};

class B : public A
{
public:
    virtual void F() override;
};
```

There is a lot of existing code that doesn't follow this yet, due to the recent addition of the
`override` keyword. The `override` keyword should be added to that code when convenient.

**`UObject`s should be passed around by pointer, not reference.**

If null is not expected by a function, this should be documented by the API or handled
appropriately. For example:

```cpp
// Bad
void AddActorToList(AActor& Obj);

// Good
void AddActorToList(AActor* Obj);
```

---

## Platform-Specific Code

Platform-specific code should always be abstracted and implemented in platform-specific source
files in appropriately named subdirectories, for example:

```text
Engine/Platforms/[PLATFORM]/Source/Runtime/Core/Private/[PLATFORM]PlatformMemory.cpp
```

In general, you should avoid adding any uses of `PLATFORM_[PLATFORM]`. For example, avoid adding
`PLATFORM_XBOXONE` to code outside of a directory named `[PLATFORM]`. Instead, extend the
hardware abstraction layer to add a static function, for example in `FPlatformMisc`:

```cpp
FORCEINLINE static int32 GetMaxPathLength()
{
    return 128;
}
```

Platforms can then override this function, returning either a platform-specific constant value
or even using platform APIs to determine the result. If you force-inline the function it has the
same performance characteristics as using a define.

In cases where a define is absolutely necessary, create new `#define` directives that describe
particular properties that can apply to a platform, for example `PLATFORM_USE_PTHREADS`. Set the
default value in `Platform.h` and override for any platforms which require it in the
platform-specific `Platform.h` file.

For example, in `Platform.h` we have:

```cpp
#ifndef PLATFORM_USE_PTHREADS
    #define PLATFORM_USE_PTHREADS 1
#endif
```

`WindowsPlatform.h` has:

```cpp
#define PLATFORM_USE_PTHREADS 0
```

Cross-platform code can then use the define directly without needing to know the platform:

```cpp
#if PLATFORM_USE_PTHREADS
    #include "HAL/PThreadRunnableThread.h"
#endif
```

We centralize the platform-specific details of the engine which allows details to be contained
entirely within platform-specific source files. Doing so makes it easier to maintain the engine
across multiple platforms, additionally you are able to port code to new platforms without the
need to scour the codebase for platform-specific defines.

Keeping platform code in platform-specific folders is also a requirement for NDA platforms such
as PlayStation, Xbox and Nintendo Switch.
