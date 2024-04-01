# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT
param(
    [string]$jsonFilePath,
    [string]$objFile
)

# Read the JSON file
$jsonContent = Get-Content -Raw -Path $jsonFilePath | ConvertFrom-Json

# Define an array of patterns that indicate system header files
$systemHeaderPatterns = @(
    "windows kits",
    "microsoft visual studio"
)

# Get the dependencies, removing system header paths
$dependencies = $jsonContent.Data.Includes | Where-Object { $dependency = $_; -not ($systemHeaderPatterns | Where-Object { $dependency -like "*$_*" }) }

# Write the dependencies in .d format
Write-Output "${objFile}: \"
foreach ($dependency in $dependencies) {
    Write-Output " $dependency \"
}
