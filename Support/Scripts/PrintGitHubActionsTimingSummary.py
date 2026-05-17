#!/usr/bin/env python3

import datetime
import json
import os
import urllib.request


def parse_time(value):
    if not value:
        return None
    return datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))


def duration_seconds(start, end):
    start_time = parse_time(start)
    end_time = parse_time(end)
    if start_time is None or end_time is None:
        return None
    return int((end_time - start_time).total_seconds())


def fmt(seconds):
    if seconds is None:
        return "running"
    minutes, remaining = divmod(seconds, 60)
    return f"{minutes}m {remaining:02d}s" if minutes else f"{remaining}s"


def main():
    repo = os.environ["GITHUB_REPOSITORY"]
    run_id = os.environ["GITHUB_RUN_ID"]
    token = os.environ["GH_TOKEN"]

    req = urllib.request.Request(
        f"https://api.github.com/repos/{repo}/actions/runs/{run_id}/jobs?per_page=100",
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )

    with urllib.request.urlopen(req) as response:
        jobs = json.load(response)["jobs"]

    job_rows = []
    step_rows = []
    for job in jobs:
        job_seconds = duration_seconds(job.get("started_at"), job.get("completed_at"))
        job_rows.append((job_seconds or 0, job["name"], fmt(job_seconds), job.get("conclusion") or job["status"]))
        for step in job.get("steps", []):
            step_seconds = duration_seconds(step.get("started_at"), step.get("completed_at"))
            if step_seconds is None:
                continue
            step_rows.append(
                (step_seconds, job["name"], step["name"], fmt(step_seconds), step.get("conclusion") or step["status"])
            )

    job_rows.sort(reverse=True)
    step_rows.sort(reverse=True)

    lines = ["## CI timing summary", "", "| Job | Duration | Result |", "| --- | ---: | --- |"]
    for _, name, duration, result in job_rows:
        lines.append(f"| {name} | {duration} | {result} |")

    lines.extend(["", "### Slowest steps", "", "| Job | Step | Duration | Result |", "| --- | --- | ---: | --- |"])
    for _, job, step, duration, result in step_rows[:15]:
        lines.append(f"| {job} | {step} | {duration} | {result} |")

    output = "\n".join(lines) + "\n"
    print(output)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write(output)


if __name__ == "__main__":
    main()
