# Text Serialization Workflows

Use this reference when an agent needs human-readable serialization.

## Read First

- [Documentation/Libraries/SerializationText.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/SerializationText.md)
- [Documentation/Libraries/Reflection.md](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md)
- [Tests/Libraries/SerializationText/SerializationJsonTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationText/SerializationJsonTest.cpp)
- [Tests/Libraries/SerializationText/JsonTokenizerTest.cpp](/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationText/JsonTokenizerTest.cpp)

## Main Entry Points

- `SC::SerializationJson`
- `SC::JsonTokenizer`

## Decision Guide

- Use JSON when humans or other tools need to inspect the data.
- Use exact mode when field order is fixed.
- Use versioned mode when old text data must still load after schema changes.
- Include the containers bridge if container fields appear in the model.

## Caveats

- JSON is the only implemented text format right now.
- The tokenizer validates structure, not a full DOM.
- The serializer depends on `Strings`.

## Good Prompts

- Write a reflected model to JSON.
- Load a JSON configuration file after a field rename.
- Explain why exact mode fails when field order changes.
