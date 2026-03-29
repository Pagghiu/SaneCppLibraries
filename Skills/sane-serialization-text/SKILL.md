---
name: sane-serialization-text
description: Serialize reflected Sane C++ models to and from text formats, especially JSON. Use when an agent needs human-readable persistence, versioned text loading, JsonTokenizer behavior, or help choosing text over binary serialization.
---

# Sane Serialization Text

## Use This Skill

Use this skill when the task is to serialize reflected data into JSON or another text format and preserve readability or interoperability.

## What To Do

- Start with [Documentation/Libraries/SerializationText.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationText.md).
- Inspect [Tests/Libraries/SerializationText/SerializationJsonTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationText/SerializationJsonTest.cpp) and [Tests/Libraries/SerializationText/JsonTokenizerTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationText/JsonTokenizerTest.cpp).
- Use `SC::SerializationJson` for JSON read and write workflows.
- Treat `JsonTokenizer` as a streaming validation layer, not a DOM parser.
- Include `sane-containers-reflection` whenever the model stores Sane containers.
- Remember that this skill depends on `Strings`.

## What To Check

- Whether the user wants exact-order loading or versioned loading.
- Whether the output must stay human readable.
- Whether the text format is JSON today or another structured format later.
- Whether the field order and naming rules are stable enough for exact mode.

## Reference

- [Text serialization workflows](references/text-format-workflows.md)
