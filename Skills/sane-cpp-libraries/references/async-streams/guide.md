# Sane Async Streams

## Quick Use

Choose `async-streams` when data must move through a pipeline and the producer or consumer can pause, resume, or transform data incrementally.

- Use `AsyncReadableStream` when the skill should produce data in chunks.
- Use `AsyncWritableStream` when the skill should consume data in chunks.
- Use `AsyncTransformStream` when the task needs a bridge between readable and writable stages.
- Use `AsyncPipeline` when several streams must be composed into one flow.
- Use the request-stream adapters when you need to wrap async requests as stream sources or sinks.

## When Not To Use

- Use `async` directly if the task is request lifecycle only.
- Use `http` or `http-client` if the user really wants HTTP and not a generic stream pipeline.

## References

- [stream pipelines and backpressure](references/stream-pipelines-and-backpressure.md)
- Public docs: `Documentation/Libraries/AsyncStreams.md`
- Tests: `Tests/Libraries/AsyncStreams/AsyncStreamsTest.cpp`
- Tests: `Tests/Libraries/AsyncStreams/AsyncRequestStreamsTest.cpp`
