# Contributing to Q1View

Thank you for your interest in contributing to Q1View.

## Reporting Bugs

Open a [GitHub Issue](https://github.com/chammoru/Q1View/issues) and include:

- Q1View version and whether you used the installer or portable package.
- Steps to reproduce, including source file type and resolution where relevant.
- Expected vs. actual behavior.
- If the issue involves pixel values, raw format tokens, or metrics, include the
  exact file name or format string used.

## Suggesting Features

Open an issue describing the use case. Explaining why the current behavior is
limiting is more useful than describing a specific implementation.

## Submitting Pull Requests

1. Fork the repository and create a branch from `master`.
2. Build and verify both applications before opening the PR (see [Build](#build)).
3. Run the core regression tests and confirm they pass (see [Tests](#tests)).
4. If you changed UI behavior, update the relevant section of
   [docs/USER_GUIDE.md](docs/USER_GUIDE.md). Replace screenshots under
   `docs/images/` with current captures when the UI changes.
5. Open the PR against `master` with a clear description of what changed and why.

## Build

Requirements: Windows x64, Visual Studio 2019 or newer with the C++ desktop
workload, and PowerShell.

```powershell
msbuild Viewer\Viewer.sln /m /restore /p:Configuration=Release /p:Platform=x64
msbuild Comparer\Comparer.sln /m /restore /p:Configuration=Release /p:Platform=x64
```

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for full dependency and packaging
details.

## Tests

The core regression suite covers raw format parsing, pixel value sampling, and
PSNR/SSIM calculations. Run it before submitting:

On macOS or Linux:
```sh
./Tests/run_core_regression_tests.sh
```

From a Visual Studio Developer PowerShell on Windows:
```powershell
msbuild Tests\CoreRegressionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
.\Tests\bin\x64\Release\CoreRegressionTests.exe
```

New raw format support or metric changes should include corresponding test
fixtures and expected values.

## Commit Messages

Use a short active-voice summary line (under 72 characters). Reference an issue
number in the PR description rather than in every commit message.

```
Support odd-width YUV raw frames
Fix Comparer launch file handling
Reduce Sync Input pan latency
```

## License

By submitting a pull request you agree that your contribution will be licensed
under the [MIT License](LICENSE) that covers this project.
