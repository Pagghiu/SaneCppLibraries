# HTTP Server Benchmark Tool

This tool compares the performance of the AsyncWebServer (from Sane C++ Libraries) against Python's built-in HTTP server.

## Features

- **Concurrent Requests**: Makes multiple simultaneous HTTP requests to stress test servers
- **Two Discovery Modes**:
  - `directory`: Lists all files in the directory and requests them
  - `crawler`: Parses `index.html` to find linked resources (images, scripts, stylesheets)
- **Statistical Stability**: Multiple iterations with warmup runs for consistent results
- **Detailed Metrics**: Measures response times, throughput, success rates, and variability
- **Automatic Comparison**: Shows performance differences between servers

## Prerequisites

- Python 3.7+
- Required packages: `aiohttp`, `beautifulsoup4`, `requests`

Install dependencies:
```bash
# Create virtual environment
python3 -m venv .venv

# Activate virtual environment
source .venv/bin/activate  # On Windows: .venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt
```

Or use the virtual environment directly:
```bash
.venv/bin/python http_benchmark.py [options]
```

## Usage

### Basic Usage

Benchmark both servers using directory listing mode:
```bash
python3 http_benchmark.py --directory /path/to/website
```

### Advanced Usage

```bash
python3 http_benchmark.py \
    --directory "/Users/YourName/Developer/Projects/website" \
    --concurrent 20 \
    --mode crawler \
    --output results.json
```

### Command Line Options

- `--directory`: **Required**. Path to the directory containing files to serve
- `--port`: Port for AsyncWebServer (default: 8080)
- `--python-port`: Port for Python HTTP server (default: 8000)
- `--concurrent`: Number of concurrent requests (default: 10)
- `--timeout`: Request timeout in seconds (default: 30)
- `--mode`: `directory` or `crawler` (default: directory)
- `--iterations`: Number of benchmark iterations for statistical stability (default: 5)
- `--warmup`: Number of warmup iterations before measurements (default: 2)
- `--delay`: Delay between iterations in seconds (default: 1.0)
- `--asyncwebserver-only`: Only benchmark AsyncWebServer
- `--python-only`: Only benchmark Python HTTP server
- `--output`: Save results to JSON file

## How It Works

1. **AsyncWebServer**: Assumes the server is already running. The script will show the command to start it.
2. **Python HTTP Server**: Automatically started by the script in a subprocess.
3. **Discovery**: Finds files to request either by directory listing or HTML parsing.
4. **Benchmarking**: Makes concurrent requests using asyncio and measures response times.
5. **Comparison**: Shows detailed statistics comparing both servers.

## Example Output

```
=== Benchmarking AsyncWebServer ===
Server URL: http://localhost:8080
Directory: /Users/YourName/Developer/Projects/website
Mode: directory
Concurrent requests: 10
Found 245 files to benchmark
Total time: 3.45s
Successful requests: 245/245
Response time - Min: 0.012s, Max: 1.234s, Avg: 0.089s
Requests/sec: 71.01
Throughput: 45.67 MB/s

=== Benchmarking Python HTTP Server ===
...

=== COMPARISON ===
AsyncWebServer: 71.01 requests/sec, 45.67 MB/s
Python HTTP Server: 45.23 requests/sec, 28.91 MB/s
AsyncWebServer is 56.9% faster in requests/sec
AsyncWebServer has 57.9% higher throughput
```

## Starting AsyncWebServer

Before running the benchmark, start your AsyncWebServer:

```bash
# Build the example
./SC.sh build compile AsyncWebServer Debug

# Run with the website directory
./SC.sh build run AsyncWebServer Debug -- --directory "/path/to/website"
```

## Interpreting Results

- **Requests/sec**: Higher is better - shows how many requests the server can handle per second
- **Throughput (MB/s)**: Higher is better - shows data transfer speed
- **Response Times**: Lower average/min/max times are better
- **Success Rate**: Should be 100% for reliable servers

Common performance issues to look for:
- High max response times (indicates queuing or blocking)
- Low throughput (network or I/O bottlenecks)
- Failed requests (server errors or timeouts)

## Troubleshooting

- **AsyncWebServer not ready**: Make sure to start it before running the benchmark
- **Port conflicts**: Use different ports with `--port` and `--python-port`
- **Large directories**: Use `--concurrent` to control load, or switch to `crawler` mode
- **Timeouts**: Increase `--timeout` for slow networks or large files
