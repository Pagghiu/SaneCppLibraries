---
name: sane-serialization-recipes
description: Choose and compose Sane C++ reflection-based serialization workflows. Use when an agent needs a decision guide for binary versus text serialization, container adapter usage, versioning strategy, or end-to-end model-to-wire recipes.
---

# Sane Serialization Recipes

## Use This Skill

Use this skill when the task is to choose the right serialization path or to connect reflection, container adapters, and a serializer into one workflow.

## What To Do

- Start with [Documentation/Libraries/Reflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md).
- Inspect [Documentation/Libraries/SerializationBinary.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationBinary.md) and [Documentation/Libraries/SerializationText.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationText.md).
- Include `sane-containers-reflection` whenever the model contains Sane containers.
- Route detailed binary workflows to `sane-serialization-binary` and detailed text workflows to `sane-serialization-text`.
- Focus on decision making, not on repeating every API detail from the lower-level skills.

## What To Check

- Whether the data is a config, cache, save game, interchange payload, or debug dump.
- Whether humans need to read it.
- Whether schema drift is expected.
- Whether the model contains Sane containers and therefore needs the adapter layer.

## Reference

- [Model to wire recipes](references/model-to-wire-recipes.md)
