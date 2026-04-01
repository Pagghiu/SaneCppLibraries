# Model To Wire Recipes

Use this reference when an agent needs a compact decision guide for Sane serialization.

## Read First

- [Documentation/Libraries/Reflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md)
- [Documentation/Libraries/SerializationBinary.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationBinary.md)
- [Documentation/Libraries/SerializationText.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationText.md)
- [Documentation/Libraries/ContainersReflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/ContainersReflection.md)

## Recipe Map

- Use reflection first for every serializable struct.
- Add the containers bridge when the model stores Sane containers.
- Choose binary for compact internal state or caches.
- Choose text for human-readable interchange or debugging.
- Use versioned loading whenever schema drift is part of the expected lifecycle.

## Decision Questions

- Will the data be read by a person?
- Must the file survive field additions, removals, or renames?
- Is the payload large enough that binary compactness matters?
- Does the model include Sane container fields?

## Good Prompts

- Choose binary or text serialization for this model.
- Build a reflected model and the adapter layer it needs.
- Explain which serializer to use for config versus cache data.
