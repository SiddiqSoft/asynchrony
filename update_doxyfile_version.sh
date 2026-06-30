#!/bin/bash
# Script to update Doxyfile with GitVersion during build process
# This script replaces the @VERSION@ placeholder with the actual version from GitVersion

set -e

# Get the version from GitVersion
# The version is typically available in the Build.BuildNumber variable in Azure Pipelines
VERSION="${GITVERSION_SEMVER:-0.0.0}"

echo "Updating Doxyfile with version: $VERSION"

# Update the Doxyfile with the version
sed -i.bak "s/@VERSION@/$VERSION/g" Doxyfile

echo "Doxyfile updated successfully"
echo "PROJECT_NUMBER is now set to: $VERSION"
