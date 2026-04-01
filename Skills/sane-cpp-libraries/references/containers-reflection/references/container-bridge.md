# Container Bridge Notes

Use this reference when container fields appear inside reflected or serialized models.

## Read First

- [Documentation/Libraries/ContainersReflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/ContainersReflection.md)
- [Documentation/Libraries/Reflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md)
- [Documentation/Libraries/SerializationBinary.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationBinary.md)
- [Documentation/Libraries/SerializationText.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationText.md)

## Key Rule

- Include the containers bridge when reflection or serialization needs to see Sane containers.

## Dependency Implication

- The bridge exists so reflection and serialization do not need to depend directly on `Containers` and `Memory`.
- Use it only when a model actually contains those container types.

## Good Prompts

- Add the adapter needed for `SC::Vector` fields.
- Make `SC::String` serializable in a reflected struct.
- Explain why a container field is not visible until the bridge header is included.
