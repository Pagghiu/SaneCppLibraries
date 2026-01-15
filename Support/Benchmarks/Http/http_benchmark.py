#!/usr/bin/env python3
"""
HTTP Server Benchmark Tool

Compares performance between AsyncWebServer (Sane C++ Libraries) and Python's simple HTTP server.
Makes concurrent requests to measure response times and throughput.
"""

import asyncio
import aiohttp
import argparse
import os
import subprocess
import sys
import time
import json
import statistics
from pathlib import Path
from urllib.parse import urljoin, urlparse
from bs4 import BeautifulSoup
import requests


class HttpBenchmark:
    def __init__(self, server_url, directory_path, concurrent_requests=10, timeout=30):
        self.server_url = server_url.rstrip('/')
        self.directory_path = Path(directory_path)
        self.concurrent_requests = concurrent_requests
        self.timeout = timeout
        self.session = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=self.timeout))
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    def discover_files_from_directory(self):
        """Discover all files in the directory recursively."""
        files = []
        for file_path in self.directory_path.rglob('*'):
            if file_path.is_file():
                # Get relative path from directory
                relative_path = file_path.relative_to(self.directory_path)
                files.append(str(relative_path))
        return files

    def discover_files_from_html(self, html_content, base_url):
        """Parse HTML content to find linked resources (images, scripts, stylesheets)."""
        soup = BeautifulSoup(html_content, 'html.parser')
        resources = []

        # Find images
        for img in soup.find_all('img', src=True):
            resources.append(urljoin(base_url, img['src']))

        # Find scripts
        for script in soup.find_all('script', src=True):
            resources.append(urljoin(base_url, script['src']))

        # Find stylesheets
        for link in soup.find_all('link', rel='stylesheet', href=True):
            resources.append(urljoin(base_url, link['href']))

        return resources

    async def fetch_url(self, url, semaphore):
        """Fetch a single URL and measure response time."""
        async with semaphore:
            start_time = time.time()
            try:
                async with self.session.get(url) as response:
                    content = await response.read()
                    end_time = time.time()
                    response_time = end_time - start_time
                    return {
                        'url': url,
                        'status': response.status,
                        'response_time': response_time,
                        'size': len(content),
                        'success': response.status == 200
                    }
            except Exception as e:
                end_time = time.time()
                response_time = end_time - start_time
                return {
                    'url': url,
                    'status': None,
                    'response_time': response_time,
                    'size': 0,
                    'success': False,
                    'error': str(e)
                }

    async def benchmark_files(self, files):
        """Benchmark multiple files concurrently."""
        semaphore = asyncio.Semaphore(self.concurrent_requests)
        tasks = []

        for file_path in files:
            url = f"{self.server_url}/{file_path}"
            task = self.fetch_url(url, semaphore)
            tasks.append(task)

        results = await asyncio.gather(*tasks, return_exceptions=True)

        # Filter out exceptions and collect successful results
        successful_results = []
        errors = []

        for result in results:
            if isinstance(result, Exception):
                errors.append(str(result))
            else:
                successful_results.append(result)

        return successful_results, errors

    async def benchmark_crawler(self):
        """Benchmark by crawling: start with index.html and follow links."""
        # First, get index.html
        index_url = f"{self.server_url}/index.html"
        try:
            async with self.session.get(index_url) as response:
                if response.status != 200:
                    print(f"Failed to fetch index.html: {response.status}")
                    return [], [f"Failed to fetch index.html: {response.status}"]
                html_content = await response.text()
        except Exception as e:
            print(f"Error fetching index.html: {e}")
            return [], [str(e)]

        # Parse HTML to find resources
        resources = self.discover_files_from_html(html_content, self.server_url + '/')

        # Also include index.html itself
        all_urls = [index_url] + resources

        # Remove duplicates and ensure URLs are within our server
        unique_urls = []
        for url in all_urls:
            if url.startswith(self.server_url) and url not in unique_urls:
                unique_urls.append(url)

        print(f"Found {len(unique_urls)} URLs to benchmark")

        # Convert URLs to relative paths for benchmarking
        files = []
        for url in unique_urls:
            parsed = urlparse(url)
            path = parsed.path.lstrip('/')
            if path:
                files.append(path)

        return await self.benchmark_files(files)


def start_python_http_server(directory_path, port=8000):
    """Start Python's simple HTTP server in a subprocess."""
    print(f"Starting Python HTTP server on port {port} serving {directory_path}")
    cmd = [sys.executable, '-m', 'http.server', str(port)]
    env = os.environ.copy()
    env['PYTHONUNBUFFERED'] = '1'

    try:
        process = subprocess.Popen(
            cmd,
            cwd=directory_path,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=env
        )
        # Wait a moment for server to start
        time.sleep(2)
        return process
    except Exception as e:
        print(f"Failed to start Python HTTP server: {e}")
        return None


def wait_for_server(url, timeout=10):
    """Wait for server to be ready."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        try:
            response = requests.get(url, timeout=1)
            if response.status_code == 200:
                return True
        except:
            pass
        time.sleep(0.5)
    return False


async def run_single_benchmark(benchmark, files, mode):
    """Run a single benchmark iteration."""
    start_time = time.time()

    if mode == 'crawler':
        results, errors = await benchmark.benchmark_crawler()
    else:  # directory mode
        results, errors = await benchmark.benchmark_files(files)

    end_time = time.time()
    total_time = end_time - start_time

    # Calculate statistics
    if results:
        successful_requests = [r for r in results if r['success']]
        response_times = [r['response_time'] for r in successful_requests]
        total_size = sum(r['size'] for r in successful_requests)

        return {
            'total_time': total_time,
            'successful_requests': len(successful_requests),
            'total_requests': len(results),
            'response_times': response_times,
            'total_size': total_size,
            'errors': errors
        }
    else:
        return None


async def run_benchmark(server_name, server_url, directory_path, mode='directory',
                       concurrent_requests=10, timeout=30, iterations=5, warmup=2, delay=1.0):
    """Run benchmark with multiple iterations for statistical stability."""
    print(f"\n=== Benchmarking {server_name} ===")
    print(f"Server URL: {server_url}")
    print(f"Directory: {directory_path}")
    print(f"Mode: {mode}")
    print(f"Concurrent requests: {concurrent_requests}")
    print(f"Iterations: {iterations} (with {warmup} warmup runs)")

    # Create benchmark instance outside the context manager to reuse across iterations
    benchmark = HttpBenchmark(server_url, directory_path, concurrent_requests, timeout)

    try:
        # Discover files once
        if mode == 'crawler':
            # For crawler mode, we need to run discovery each time since it involves HTTP requests
            files = None
        else:  # directory mode
            files = benchmark.discover_files_from_directory()
            print(f"Found {len(files)} files to benchmark")

        # Warmup runs
        if warmup > 0:
            print(f"\nRunning {warmup} warmup iterations...")
            for i in range(warmup):
                async with benchmark:
                    if mode == 'crawler':
                        await benchmark.benchmark_crawler()
                    else:
                        await benchmark.benchmark_files(files)
                if delay > 0:
                    await asyncio.sleep(delay)

        # Measurement runs
        print(f"\nRunning {iterations} measurement iterations...")
        all_stats = []

        for i in range(iterations):
            print(f"Iteration {i+1}/{iterations}...", end=' ', flush=True)

            async with benchmark:
                result = await run_single_benchmark(benchmark, files, mode)
            if result:
                # Calculate per-iteration stats
                rps = result['successful_requests'] / result['total_time'] if result['total_time'] > 0 else 0
                throughput = (result['total_size'] / (1024 * 1024)) / result['total_time'] if result['total_time'] > 0 else 0

                all_stats.append({
                    'requests_per_second': rps,
                    'throughput_mbps': throughput,
                    'total_time': result['total_time'],
                    'response_times': result['response_times']
                })

                print(f"{rps:.3f}")

            if delay > 0 and i < iterations - 1:
                await asyncio.sleep(delay)

        if not all_stats:
            print("No successful measurements obtained")
            return None

        # Calculate aggregate statistics
        rps_values = [s['requests_per_second'] for s in all_stats]
        throughput_values = [s['throughput_mbps'] for s in all_stats]
        all_response_times = [rt for s in all_stats for rt in s['response_times']]

        # Use median for more stable results (less sensitive to outliers)
        median_rps = statistics.median(rps_values)
        median_throughput = statistics.median(throughput_values)

        # Calculate coefficient of variation (CV) for stability assessment
        rps_cv = statistics.stdev(rps_values) / statistics.mean(rps_values) if len(rps_values) > 1 else 0
        throughput_cv = statistics.stdev(throughput_values) / statistics.mean(throughput_values) if len(throughput_values) > 1 else 0

        stats = {
            'server_name': server_name,
            'iterations': iterations,
            'warmup_runs': warmup,
            'total_requests': len(all_response_times),  # Total number of individual requests across all iterations
            'median_requests_per_second': median_rps,
            'median_throughput_mbps': median_throughput,
            'rps_mean': statistics.mean(rps_values),
            'rps_stddev': statistics.stdev(rps_values) if len(rps_values) > 1 else 0,
            'rps_cv': rps_cv,
            'throughput_mean': statistics.mean(throughput_values),
            'throughput_stddev': statistics.stdev(throughput_values) if len(throughput_values) > 1 else 0,
            'throughput_cv': throughput_cv,
            'min_response_time': min(all_response_times) if all_response_times else 0,
            'max_response_time': max(all_response_times) if all_response_times else 0,
            'avg_response_time': statistics.mean(all_response_times) if all_response_times else 0,
            'response_time_median': statistics.median(all_response_times) if all_response_times else 0,
            'all_rps_values': rps_values,
            'all_throughput_values': throughput_values
        }

        print(f"\n--- Results ---")
        print(f"Median Requests/sec: {median_rps:.2f}")
        print(f"Median Throughput: {median_throughput:.2f} MB/s")
        print(f"Response time - Min: {stats['min_response_time']:.3f}s, Max: {stats['max_response_time']:.3f}s, Median: {stats['response_time_median']:.3f}s")
        print(f"Stability - RPS CV: {rps_cv:.3f}, Throughput CV: {throughput_cv:.3f}")

        if rps_cv > 0.1 or throughput_cv > 0.1:
            print("Warning: High variability detected (>10% CV). Consider increasing --iterations or --delay.")

        return stats
    finally:
        # Clean up benchmark instance
        if benchmark.session:
            await benchmark.session.close()


def main():
    parser = argparse.ArgumentParser(description='HTTP Server Benchmark Tool')
    parser.add_argument('--directory', required=True,
                       help='Directory to serve files from')
    parser.add_argument('--port', type=int, default=8080,
                       help='Port for AsyncWebServer (default: 8080)')
    parser.add_argument('--python-port', type=int, default=8000,
                       help='Port for Python HTTP server (default: 8000)')
    parser.add_argument('--concurrent', type=int, default=10,
                       help='Number of concurrent requests (default: 10)')
    parser.add_argument('--timeout', type=int, default=30,
                       help='Request timeout in seconds (default: 30)')
    parser.add_argument('--mode', choices=['directory', 'crawler'], default='directory',
                       help='Benchmark mode: directory (list all files) or crawler (parse HTML for resources)')
    parser.add_argument('--iterations', type=int, default=5,
                       help='Number of benchmark iterations for statistical stability (default: 5)')
    parser.add_argument('--warmup', type=int, default=2,
                       help='Number of warmup iterations before measurements (default: 2)')
    parser.add_argument('--delay', type=float, default=1.0,
                       help='Delay between iterations in seconds (default: 1.0)')
    parser.add_argument('--asyncwebserver-only', action='store_true',
                       help='Only benchmark AsyncWebServer (assume it is already running)')
    parser.add_argument('--python-only', action='store_true',
                       help='Only benchmark Python HTTP server')
    parser.add_argument('--output', help='Output results to JSON file')

    args = parser.parse_args()

    directory_path = Path(args.directory)
    if not directory_path.exists() or not directory_path.is_dir():
        print(f"Error: Directory {directory_path} does not exist")
        return 1

    results = {}

    # Benchmark AsyncWebServer
    if not args.python_only:
        asyncwebserver_url = f"http://localhost:{args.port}"
        print(f"Assuming AsyncWebServer is running on {asyncwebserver_url}")
        print("Make sure to start it with:")
        print(f"./SC.sh build run AsyncWebServer Debug -- --directory \"{directory_path}\"")
        print()

        if wait_for_server(asyncwebserver_url):
            async_result = asyncio.run(run_benchmark(
                "AsyncWebServer",
                asyncwebserver_url,
                directory_path,
                args.mode,
                args.concurrent,
                args.timeout,
                args.iterations,
                args.warmup,
                args.delay
            ))
            results['asyncwebserver'] = async_result
        else:
            print("AsyncWebServer not ready, skipping...")
            results['asyncwebserver'] = None

    # Benchmark Python HTTP server
    if not args.asyncwebserver_only:
        python_port = args.python_port
        python_url = f"http://localhost:{python_port}"

        python_process = start_python_http_server(directory_path, python_port)

        if python_process and wait_for_server(python_url):
            try:
                python_result = asyncio.run(run_benchmark(
                    "Python HTTP Server",
                    python_url,
                    directory_path,
                    args.mode,
                    args.concurrent,
                    args.timeout,
                    args.iterations,
                    args.warmup,
                    args.delay
                ))
                results['python'] = python_result
            finally:
                # Clean up Python server
                python_process.terminate()
                python_process.wait()
        else:
            print("Failed to start Python HTTP server")
            results['python'] = None

    # Compare results
    if results.get('asyncwebserver') and results.get('python'):
        print("\n=== COMPARISON ===")
        async_stats = results['asyncwebserver']
        python_stats = results['python']

        async_rps = async_stats['median_requests_per_second']
        python_rps = python_stats['median_requests_per_second']

        async_throughput = async_stats['median_throughput_mbps']
        python_throughput = python_stats['median_throughput_mbps']

        print(f"AsyncWebServer: {async_rps:.2f} requests/sec, {async_throughput:.2f} MB/s")
        print(f"Python HTTP Server: {python_rps:.2f} requests/sec, {python_throughput:.2f} MB/s")

        if async_rps > python_rps:
            rps_diff = ((async_rps - python_rps) / python_rps) * 100
            print(f"AsyncWebServer is {rps_diff:.1f}% faster in requests/sec")
        else:
            rps_diff = ((python_rps - async_rps) / async_rps) * 100
            print(f"Python HTTP Server is {rps_diff:.1f}% faster in requests/sec")

        if async_throughput > python_throughput:
            throughput_diff = ((async_throughput - python_throughput) / python_throughput) * 100
            print(f"AsyncWebServer has {throughput_diff:.1f}% higher throughput")
        else:
            throughput_diff = ((python_throughput - async_throughput) / async_throughput) * 100
            print(f"Python HTTP Server has {throughput_diff:.1f}% higher throughput")

    # Save results to file if requested
    if args.output:
        with open(args.output, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"\nResults saved to {args.output}")

    return 0


if __name__ == '__main__':
    sys.exit(main())
