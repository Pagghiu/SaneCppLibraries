# Stream Pipelines And Backpressure

## Use This When

- Connect a producer to a consumer with incremental chunks.
- Transform data without buffering the whole payload.
- Keep flow control explicit when memory is fixed or bounded.

## Core Types

- `SC::AsyncReadableStream`
- `SC::AsyncWritableStream`
- `SC::AsyncTransformStream`
- `SC::AsyncPipeline`
- `SC::AsyncRequestReadableStream`
- `SC::AsyncRequestWritableStream`

## Mental Model

- Readable streams produce buffers and end or error explicitly.
- Writable streams consume buffers and signal completion explicitly.
- Transform streams bridge the two sides and can apply processing in the middle.
- Pipelines connect multiple stages and propagate end or error state.

## Common Patterns

- Wrap async file or socket requests as streams when the caller wants stream semantics.
- Use a transform stage for compression, conversion, filtering, or framing.
- Keep buffer ownership and refcounting in mind when pushing or unrefing data.

## Pitfalls

- Do not assume unlimited buffering.
- Pause or resume when the buffer pool is full.
- Use the request-stream adapters only when the source of data is an async request.

## Best Sources

- `Documentation/Libraries/AsyncStreams.md`
- `Tests/Libraries/AsyncStreams/AsyncStreamsTest.cpp`
- `Tests/Libraries/AsyncStreams/AsyncRequestStreamsTest.cpp`
- `Tests/Libraries/AsyncStreams/ZLibStreamTest.cpp`
