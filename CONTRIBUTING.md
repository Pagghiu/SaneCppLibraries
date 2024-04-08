# Sane C++ Contributing Guidelines

> Note: this document will be updated regularly clarifying existing rules and adding missing guidelines that will emerge from discussions or PRs being reviewed.

Before deep diving in this document, please take some time to read the [Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html) and [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html).  
More importantly browse the existing codebase!

## TLDR

Look at existing libraries in the repo and try to harmonically blend-in your new code 🙂

## Maintainers

Currently the only maintainer of the library is [Stefano Cristiano (@Pagghiu)](http://github.com/pagghiu).

## Discussing

Please, before committing to any large body of work describe your intentions getting in touch in any of the following ways.

And very importantly, try your best to be nice when interacting with other people about this project 😊.

- [Sane Coding Discord](https://discord.gg/tyBfFp33Z6)
- [Twitter](https://twitter.com/pagghiu_) `@pagghiu_`
- [Mastodon](https://mastodon.gamedev.place/@pagghiu) `@pagghiu`
- [Github Discussion](https://github.com/Pagghiu/SaneCppLibraries/discussions)

Alternatively I am also reading the following discords too:
- [Italian C++ Discord](https://discord.gg/GPATr8QxfS) (`@Pagghiu` from any appropriate channel or just a DM, english and italian are both fine)
- [Handmade Network Discord](https://discord.gg/hmn) (`@Pagghiu` from any appropriate channel or just a DM)

## Finding something to do

- Take a look at the [good first issue](https://github.com/Pagghiu/SaneCppLibraries/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) list.
- Just grep the project for `TODO:` and pick one you like 😄.  
- Also take a look at each specific [Library](https://pagghiu.github.io/SaneCppLibraries/libraries.html) page **Roadmap**.  


> I highly suggest to create a discussion (on discord or github discussion or in the issue tracker) on any contribution you are planning to share!

In general outside of the above, it's highly likely to be accepted any PR doing:

- Bug fixing
- Implementation of existing Libraries on unsupported OS
- Wrapping any library to make it easily usable from C

## Do not expand project scope

For now project scope is defined by the existing set of libraries.  
The main objective is currently making them production ready on all supported Operating Systems.

In other words this means that until the above objective will be reached any new library proposal will not be accepted.  
Discussing ideas for future libraries is fine, and it's better if done in advance to properly identify its features.

## Avoid feature creep

Even for existing libraries, there is no intention of covering [everyone's use cases](https://xkcd.com/927/).  
The general guideline is focusing on a small enough subset of features covering `90%` of common use cases.

## No external dependencies

This project encourages a _from scratch_ attitude when writing new code.

It's also positively accepted any use of existing Operating System API (properly wrapped) if such functionality is readily available on all supported OS.  
`macOS` and `Windows` are easy to support in this regard as each version identifies a precise set of APIs that are available.

> Exceptions will need to be made on `Linux` as some functionality is not part of the kernel, and presence of a given user-land library is not guaranteed.  
The exact mechanism to deliver such dependencies is still to be investigated,  defined and regulated.
In any case Linux dependencies will need to be wrapped so that they will still not require or impose any build system. 

## OS Specific code

Operating System specific code should be isolated in dedicated libraries and not spread throughout the codebase.

## Testing

Any new code added should be tested with `90%`+ code coverage.

## Language

- The project official language is American English
- Try to user proper spelling and grammar
- Please avoid any offensive wording

## Commit message format

Every commit should:
- start with the name of the library being touched followed by short description
- Where necessary add a dot and a new line and write further paragraph describing the details of the change
- Use imperative tone (`Use` instead of `used` etc.)

Good:
- `SerializationBinary: Use pointer to field directly instead of reinterpret_cast-ing`
- `FileSystem: Add option to skip empty entries to Path::join`

Bad:
- `Fix stuff`
- `FileSystem: Added Path::join option`

## Git workflow

The project uses a `rebase` workflow to keep linear history.  
PRs will be rebased on top of latest master TIP when it will be merged.

## Naming things

Naming things is one of the hardest task in Computer Science (together with Cache Invalidation) and also extremely important to make a library easy to use.  

For this reason, consider renaming structs / methods / variables after you will have finished building the functionality.
Often their real essence will now be evident (possibly before sending a PR).

## Code guidelines

- Write code that harmonically blends with existing code
- Split changes in multiple commits if it clarifies your intents
- Check spelling (see [Language](#language))
- Use the same patterns you can recognize being already used in existing code
- Write code that adheres to the project [License](License.txt)

Do not:
- Do not make huge and large impacting changes without having agreed them with project [Maintainers](#maintainers)
- Do not pointlessly move existing code around for no reason
- Do not leave commented-out code in your commits

## Issues Guidelines

- Try keeping one issue per PR
- Clearly identify the objective of PR with a meaningful name for it
- If needed add any text to further clarify the intent of the PR

## Comparisons

Comparisons are often carriers of negative sentiments that this project wants to avoid.  
They are sometimes made to demonstrate _superiority over others_ or to _crush the competition_ and express similar negative thoughts that are not in the interest of this project. 

`Sane C++` doesn't compete but offers an alternative vision on how to program in a subset of C++ that is simple and quick to compile. Anyone liking this vision will use the library, with or without other libraries.  
Those who feel particularly inspired might support the project with a PR or even just a star or like 😌.

It would also be very difficult to compare libraries that have very different and distant scopes, goals, constraints, and levels of maturity.
It's always possible to create a test or to identify some criteria where one or the other might be better.

[Benchmarks](#benchmarks) are the only allowed form of comparison.

## Benchmarks

Benchmarks are the only allowed form of [comparison](#comparisons).

They should be without bias and built with objectivity and completeness.  
It's not yet the right time to do them as everything is still very much in draft state.
They will be considered in the future for some specific Libraries (`SC::Async` and `SC::Http` for example) when they will be mature enough.

It's preferable adding a `Sane C++` implementation to an existing benchmark, already comparing other libraries, rather then creating a new one.