# Docker example

This example provides a sample `Dockerfile` for building llfix inside a Linux container.

The image uses Rocky Linux 9 because it is close to RHEL.

The 'Dockerfile' installs the required build tools, clones the open-source llfix repository, and builds the sample FIX client and FIX server examples.

Tested with Docker version 29.4.3.