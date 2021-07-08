# OpenSim Creator <img src="resources/logo.png" align="right" alt="OpenSim Creator Logo" width="128" height="128" />

> A thin UI for building OpenSim models

![screenshot](screenshot.png)

OpenSim Creator (`osc`) is a standalone UI for building
[OpenSim](https://github.com/opensim-org/opensim-core) models. It is
designed as a proof-of-concept GUI with the intent that some of its
features may be merged into the official [OpenSim GUI](https://github.com/opensim-org/opensim-gui).

Architectrually, `osc` mostly uses C/C++ that is directly integrated
against the [OpenSim core API](https://github.com/opensim-org/opensim-core) and otherwise only
uses lightweight open-source libraries (e.g. SDL2, GLEW, and ImGui) that can be built from source
on all target platforms. This makes `osc` fairly easy to build, integrate, and package.

`osc` started development in 2021 in the [Biomechanical Engineering](https://www.tudelft.nl/3me/over/afdelingen/biomechanical-engineering)
department at [TU Delft](https://www.tudelft.nl/). It is funded by the
Chan Zuckerberg Initiative's "Essential Open Source Software for
Science" grant (Chan Zuckerberg Initiative DAF, 2020-218896 (5022)).

| <img src="resources/tud_logo.png" alt="TUD logo" width="128" height="128" /> | <img src="resources/chanzuckerberg_logo.png" alt="CZI logo" width="128" height="128" /> |
| - | - |
| [Biomechanical Engineering at TU Delft](https://www.tudelft.nl/3me/over/afdelingen/biomechanical-engineering) | [The Chan Zuckerberg Initiative](https://chanzuckerberg.com/) |


# 🚀 Installation

> 🚧 **ALPHA-STAGE SOFTWARE** 🚧: OpenSim Creator is currently in development, so
> things are prone to breaking. If a release doesn't work for you,
> report it on the [issues](issues)
> page, try a different [release](releases)
> or try downloading the latest passing build from [the actions page](actions)

Below is a list of the latest releases of `osc`. This list is updated manually whenever a new
release is published. If you want bleeding-edge releases, download a passing build from
[the actions page](actions). The installers should not require any additional software to be
installed. If you find they do, [report it](issues).

*Latest release: 0.0.2 (released 12th Apr 2021, uses OpenSim 4.2)*

| OS | Link | Comments |
| - | - | - |
| Windows 10 | [.exe](releases/download/0.0.2/osmv-0.0.2-win64.exe) | |
| Mac (Catalina onwards) | [.dmg](releases/download/0.0.2/osmv-0.0.2-Darwin.dmg) | The DMG is unsigned, so you will probably need to open Finder -> Applications -> right click osc -> open -> go past the security warning |
| Ubuntu Focal (20) | [.deb](releases/download/0.0.2/osmv_0.0.2_amd64.deb) | |
| Debian Buster (10) | [.deb](releases/download/0.0.2/osmv_0.0.2_amd64.deb) | |


# 🏗️  Building

No full-fat build documentation available (yet ;)). However, you can
*probably* just run the CI build scripts because they don't rely on any
GitHub (CI) specific tricks (e.g. see [action](.github/workflows/continuous-integration-workflow.yml)).

These build scripts should work on a standard C++ developer's machine (as long as you have a C/C++
compiler, CMake, etc. installed):

| OS | Build Script | Usage Example |
| - | - | - |
| Windows | [.bat](scripts/build_windows.bat) | `git clone https://github.com/adamkewley/osmv && cd osmv && scripts\build_windows.bat` |
| Mac | [.sh](scripts/build_mac-catalina.sh) | `git clone https://github.com/adamkewley/osmv && cd osmv && scripts/build_mac-catalina.sh` |
| Ubuntu/Debian | [.sh](scripts/build_debian-buster.sh) | `git clone https://github.com/adamkewley/osmv && cd osmv && scripts/build_debian-buster.sh` |
