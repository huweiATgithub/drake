---
title: Source Installation
---

# Supported Configurations

The following table shows the configurations and platforms that Drake
officially supports:

<!-- The operating system requirements should match those listed in both the
     root CMakeLists.txt and tools/workspace/os.bzl. -->
<!-- The minimum compiler versions should match those listed in both the root
     CMakeLists.txt and tools/workspace/cc/repository.bzl. -->

| Operating System ⁽¹⁾               | Architecture | Python ⁽²⁾ | Bazel | CMake | C/C++ Compiler ⁽³⁾           | Java                          |
|------------------------------------|--------------|------------|-------|-------|------------------------------|-------------------------------|
| Ubuntu 20.04 LTS (Focal Fossa)     | x86_64       | 3.8        | 6.3   | 3.16  | GCC 9 (default) or Clang 12  | OpenJDK 11                    |
| Ubuntu 22.04 LTS (Jammy Jellyfish) | x86_64       | 3.10       | 6.3   | 3.22  | GCC 11 (default) or Clang 12 | OpenJDK 11                    |
| macOS Monterey (12)                | x86_64       | 3.11       | 6.3   | 3.25  | Apple LLVM 14 (Xcode 14)     | AdoptOpenJDK 16 (HotSpot JVM) |
| macOS Monterey (12)                | arm64        | 3.11       | 6.3   | 3.25  | Apple LLVM 14 (Xcode 14)     | AdoptOpenJDK 16 (HotSpot JVM) |
| macOS Ventura (13)                 | arm64        | 3.11       | 6.3   | 3.26  | Apple LLVM 14 (Xcode 14)     | AdoptOpenJDK 16 (HotSpot JVM) |

"Official support" means that we have Continuous Integration test coverage to
notice regressions, so if it doesn't work for you then please file a bug report.

Unofficially, Drake is also likely to be compatible with newer versions of
Ubuntu or macOS than what are listed, or with Ubuntu 22.04 running on arm64, or
with other versions of Python or Java. However, these are not supported so if it
doesn't work for you then please file a pull request with the fix, not a bug
report.

All else being equal, we would recommend developers use Ubuntu 22.04 (Jammy).

⁽¹⁾ Drake features that perform image rendering (e.g., camera simulation)
require a working display server. Most personal computers will have this
already built in, but some cloud or docker environments may require extra
setup steps.

⁽²⁾ CPython is the only Python implementation supported.

⁽³⁾ Drake requires a compiler running in C++17 or C++20 mode.

# Getting Drake

Run:

```
git clone --filter=blob:none https://github.com/RobotLocomotion/drake.git
```

Note: we suggest you keep the default clone directory name (``drake``) and not
rename it (such as ``drake2``).  The CLion integration will suffer if the
checkout directory is not named ``drake``.  (See [CLion IDE setup](clion.html) for details.)

Note: the build process may encounter problems if you have unusual characters
like parentheses in the absolute path to the drake directory
(see [#394](https://github.com/RobotLocomotion/drake/issues/394)).

## Using a fork of Drake

The above ``git clone`` command will configure Drake's primary repository as a
remote called ``origin``. If you plan to fork Drake for development, we
recommend that you configure your fork of Drake's primary repository as the
``origin`` remote and Drake's primary repository as the ``upstream``
remote. This can be done by executing the following commands:

```
cd drake
git remote set-url origin git@github.com:[your github user name]/drake.git
git remote add upstream https://github.com/RobotLocomotion/drake.git
git remote set-url --push upstream no_push
```

We recommend that you
[setup SSH access to github.com](https://help.github.com/articles/adding-a-new-ssh-key-to-your-github-account/)
to avoid needing to type your password each time you access it.

# Mandatory platform-specific instructions

Before running the build, you must follow some one-time platform-specific
setup steps.

*Ubuntu:*

```
sudo ./setup/ubuntu/install_prereqs.sh
```

*macOS:*

We assume that you have already installed Xcode
([from the Mac App Store](https://itunes.apple.com/us/app/xcode/id497799835)).

After that, run:

```
./setup/mac/install_prereqs.sh
```

# Build with Bazel

For instructions, jump to
[Developing Drake using Bazel](/bazel.html#developing-drake-using-bazel),
or check out the full details at:

* [Bazel build system](/bazel.html)

## Building the Python Bindings

To use the Python bindings from Drake externally, we recommend using CMake.
As an example:

```bash
git clone https://github.com/RobotLocomotion/drake.git
mkdir drake-build
cd drake-build
cmake ../drake
make -j
```

Note that a concurrency limit passed to `make` (e.g., `make -j 2`) has almost no
effect on the Drake build. You might need to add a bazel configuration dotfile
to your home directory if your build is running out of memory. See the
[troubleshooting](/troubleshooting.html#build-oom) page for details.

Be aware that repeatedly running `make` will install the recompiled version of
Drake *on top of* the prior version. This will lead to disaster unless the set
of installed filenames is exactly the same (because old files will be hanging
around polluting your PYTHONPATH). It is safe if you are merely tweaking a
source code file and repeatedly installing, without any changes to the build
system. For any kind of larger change (e.g., upgrading to a newer Drake), we
strongly advise that you delete the prior tree (within the `install`
sub-directory) before running `make`.

Please note the additional CMake options which affect the Python bindings:

* ``-DWITH_GUROBI={ON, [OFF]}`` - Build with Gurobi enabled.
* ``-DWITH_MOSEK={ON, [OFF]}`` - Build with MOSEK™ enabled.
* ``-DWITH_SNOPT={ON, [OFF]}`` - Build with SNOPT enabled.

``{...}`` means a list of options, and the option surrounded by ``[...]`` is
the default option. An example of building ``pydrake`` with both Gurobi and
MOSEK™, without building tests:

```bash
cmake -DWITH_GUROBI=ON -DWITH_MOSEK=ON ../drake
```

You will also need to have your ``PYTHONPATH`` configured correctly.

*Ubuntu 20.04 (Focal):*

```bash
cd drake-build
export PYTHONPATH=${PWD}/install/lib/python3.8/site-packages:${PYTHONPATH}
```

*Ubuntu 22.04 (Jammy):*

```bash
cd drake-build
export PYTHONPATH=${PWD}/install/lib/python3.10/site-packages:${PYTHONPATH}
```

*macOS:*

```bash
cd drake-build
export PYTHONPATH=${PWD}/install/lib/python3.9/site-packages:${PYTHONPATH}
```
