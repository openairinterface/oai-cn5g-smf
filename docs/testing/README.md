# Local SMF Unit Testing

Testing via Docker isolates all build dependencies and provides a clean, reproducible environment for running unit tests locally without modifying your host system.

## Quick Start (Docker Workflow)

### 1. Build the Checks Image
This command compiles the SMF code and executes the unit test suite during the Docker image build stage:

```bash
docker build --target checks -f docker/Dockerfile.smf.ubuntu -t oai-smf-checks .
```

### 2. Inspect Build-Time Test Logs
Because CTest runs during the `docker build` process, test execution logs are stored directly inside the resulting image. You can read the build-time log without re-running the test suite:

```bash
docker run --rm oai-smf-checks cat /openair-smf/build/smf/build/Testing/Temporary/LastTest.log
```

> **Tip:** If you want Docker to stream all test output live to your terminal *while* building, pass the `--progress=plain` flag:
> ```bash
> docker build --progress=plain --target checks -f docker/Dockerfile.smf.ubuntu -t oai-smf-checks .
> ```

### 3. Run the Unit Tests Interactively
To re-run the unit test binary interactively (with colorized terminal output, execution statistics, or custom GTest filters):

```bash
docker run --rm oai-smf-checks /openair-smf/build/smf/build/smf_app/test/smf_unit_tests
```

---

## Advanced Options

* **Run a Specific Test Suite:**
  Filter tests directly using GoogleTest arguments:
  ```bash
  docker run --rm oai-smf-checks /openair-smf/build/smf/build/smf_app/test/smf_unit_tests --gtest_filter=SmfPolicyManagerParse.*
  ```

* **Run via CTest Verbose Output:**
  Execute CTest inside the container to view full stdout/stderr log details per test label:
  ```bash
  docker run --rm -w /openair-smf/build/smf/build oai-smf-checks ctest --verbose -L unit
  ```