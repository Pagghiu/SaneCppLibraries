# Test Layout And Invocation

Use this reference when a user needs to understand the Sane test framework or where to find library examples.

## Teach First

- `Tests/SCTest` holds the overall test harness.
- `Tests/Libraries/*` holds library-specific tests.
- Test files are often the clearest examples of how a library is meant to be used.

## Best Files To Inspect

- `Libraries/Testing/Testing.h`
- `Libraries/Testing/Testing.cpp`
- `Tests/SCTest/*`
- `Tests/Libraries/*`

## Good Advice To Give

- Use tests as example code when the docs are thin.
- Point users to the relevant library test before inventing a new code sample.
- Keep test sections narrow so one test can demonstrate more than one scenario.
