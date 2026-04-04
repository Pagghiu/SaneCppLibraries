# Sane Serialization Recipes

## Use This Guide

Use this guide when the task is to choose the right serialization path or to connect reflection, container adapters, and a serializer into one workflow.

## What To Do

- Start with `Documentation/Libraries/Reflection.md`.
- Inspect `Documentation/Libraries/SerializationBinary.md` and `Documentation/Libraries/SerializationText.md`.
- Include `containers-reflection` whenever the model contains Sane containers.
- Route detailed binary workflows to `serialization-binary` and detailed text workflows to `serialization-text`.
- Focus on decision making, not on repeating every API detail from the lower-level skills.

## What To Check

- Whether the data is a config, cache, save game, interchange payload, or debug dump.
- Whether humans need to read it.
- Whether schema drift is expected.
- Whether the model contains Sane containers and therefore needs the adapter layer.

## Reference

- [Model to wire recipes](references/model-to-wire-recipes.md)
