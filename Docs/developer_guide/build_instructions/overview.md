#  Overview

Building Slicer is the process of obtaining a copy of the source code of the project and use tools, such as compilers, project generators and build systems, to create binary libraries and executables. Slicer documentation is also generated in this process.

Users of Slicer application and extensions do not need to build the application and they can download and install pre-built packages instead. Python scripting and development of new Slicer modules in Python does not require building the application either. Only software developers interested in developing Slicer [modules](../../user_guide/modules/index.md) in C++ language or contributing to the development of Slicer core must build the application.

Slicer is based on a *superbuild* architecture. This means that the in the building process, most of the dependencies of Slicer will be downloaded in local directories (within the Slicer build directory) and will be configured, built and installed locally, before Slicer itself is built. This helps reducing the complexity for developers.

As Slicer is continuously developed, build instructions may change, too. Therefore, it is recommended to use build instructions that have the same version as the source code.

## Bundled Python requirements

PyPI packages installed by the SuperBuild are declared in
`SuperBuild/Python/*requirements.txt`, one manifest per external project
(for example, `python-numpy-requirements.txt`).
These files are the source of truth for package versions and SHA-256 hashes.
The corresponding `SuperBuild/External_python*.cmake` files copy them into the
build directory using `configure_file(COPYONLY)` and install them with
`pip --require-hashes`. Editing a manifest causes CMake to reconfigure; the
existing external-project installation steps still control when packages are
installed. System-package builds continue to use the installed system packages.

Use `Utilities/Scripts/python_package_updater.py` with PythonSlicer from a current
Slicer preview to update versions and wheel hashes. Its default search directory
is `SuperBuild`, including the nested requirements files. Keep the paired
`# [packagename]` and `# [/packagename]` markers used by the updater.
Run `python_package_updater.py --validate` to check these markers.
Review dependency compatibility and wheel availability on all supported
platforms before accepting any update.

Documentation dependencies are maintained separately in `requirements-docs.txt`.
Source-built dependencies such as VTK are not represented by these PyPI manifests.

## Extensions for developer builds of Slicer

In general, Slicer versions that a developer builds on his own computer are not expected to work with extensions in the Extensions Server.

Often the Extensions Manager does not show any extensions for a developer build. The reason is that extensions are only built for one Slicer version a day, and so there are many Slicer versions for that no extensions are available.

Even if the Extensions Manager is not empty, the listed extensions are not expected to be compatible with developer builds, created on a different computer, in a different build environment or build options, due to porential ABI incompatibility issues.

If a developer builds the Slicer application then it is expected that the developer will also build all the extension he needs. Building all the extensions after Slicer build is completed [is a simple one-line command](../extensions.md#build-test-and-package). It is also possible to just [build selected extensions](../extensions.md#build-an-extension).

## Build options

The Slicer build can be turned on/off using CMake variables, for example to reduce build time when
a feature is not needed. Some of the most commonly used ones:

- `BUILD_TESTING`: build the test suite (ON/OFF).
- `Slicer_BUILD_DICOM_SUPPORT`: build Slicer with DICOM support (ON/OFF).
- `Slicer_BUILD_DIFFUSION_SUPPORT`: build Slicer with diffusion (DWI, DTI) support (ON/OFF).
- `Slicer_BUILD_I18N_SUPPORT`: build Slicer with internationalization support (ON/OFF).
- `Slicer_BUILD_WEBENGINE_SUPPORT`: build Slicer with Qt WebEngine support (ON/OFF).
- `Slicer_BUILD_QTLOADABLEMODULES`: build Slicer Qt loadable modules (ON/OFF).
- `Slicer_BUILD_QTSCRIPTEDMODULES`: build Slicer Python Qt modules. Requires `Slicer_USE_PYTHONQT` (ON/OFF).
- `Slicer_USE_PYTHONQT`: integrate a Python-Qt interpreter into Slicer (ON/OFF).
- `Slicer_USE_NUMPY` / `Slicer_USE_SCIPY`: build Slicer with NumPy / SciPy. Requires `Slicer_USE_PYTHONQT` (ON/OFF).
- `Slicer_USE_SimpleITK`: build Slicer with SimpleITK support. Requires `Slicer_USE_PYTHONQT` (ON/OFF).
- `Slicer_BUILD_EXTENSIONMANAGER_SUPPORT`: build the Slicer extensions manager (ON/OFF).
- `Slicer_BUILD_APPLICATIONUPDATE_SUPPORT`: build Slicer with application update support (ON/OFF).
- `Slicer_VTK_VERSION_MAJOR` / `Slicer_VTK_VERSION_MINOR`: VTK version to build against.
- `Slicer_BUILD_DOCUMENTATION`: build documentation (Doxygen, Sphinx, ...) (ON/OFF).

These and other options are declared, with their default values, in the top-level
[CMakeLists.txt](https://github.com/Slicer/Slicer/blob/main/CMakeLists.txt), which remains the
authoritative source for build options.

## Custom builds

Customized editions of Slicer can be generated without changing Slicer source code, just by modifying CMake variables:

- `SlicerApp_APPLICATION_NAME`: Custom application name to be used, instead of default "Slicer". The name is the internal, programmatic name of the application.
- `SlicerApp_APPLICATION_DISPLAY_NAME`: Custom application name to be used in display situations. The name is used in user facing text such as window title bar, installation directory name, desktop shortcut names etc.
- `Slicer_DISCLAIMER_AT_STARTUP`: String that is displayed to the user after first startup of Slicer after installation (disclaimer, welcome message, etc).
- `Slicer_DEFAULT_HOME_MODULE`: Module name that is activated automatically on application start.
- `Slicer_DEFAULT_FAVORITE_MODULES`: Modules that will be added to the toolbar by default for easy access. List contains module names, separated by space character.
- `Slicer_CLIMODULES_DISABLED`: Built-in CLI modules that will be removed from the application. List contains module names, separated by semicolon character.
- `Slicer_QTLOADABLEMODULES_DISABLED`: Built-in Qt loadable modules that will be removed from the application. List contains module names, separated by semicolon character.
- `Slicer_QTSCRIPTEDMODULES_DISABLED`: Built-in scripted loadable modules that will be removed from the application. List contains module names, separated by semicolon character.
- `Slicer_USE_PYTHONQT_WITH_OPENSSL`: enable/disable building the application with SSL support (ON/OFF)
- `Slicer_USE_DCMTK_WITH_OPENSSL`: enable/disable building DCMTK with SSL support (ON/OFF)
- `Slicer_USE_SimpleITK`: enable/disable SimpleITK support (ON/OFF)
- `Slicer_BUILD_SimpleFilters`: enable/disable building SimpleFilters. Requires SimpleITK. (ON/OFF)
- `Slicer_EXTENSION_SOURCE_DIRS`: Defines additional extensions that will be included in the application package as built-in modules. Full paths of extension source directories has to be specified, separated by semicolons.
- `Slicer_BUILD_WIN32_CONSOLE_LAUNCHER`: Show/hide console (terminal window) on Windows.

Moreoptions are listed in CMake files, such as in [SlicerApplicationOptions.cmake](https://github.com/Slicer/Slicer/blob/main/CMake/SlicerApplicationOptions.cmake) and further customization is achievable by using [SlicerCustomAppTemplate](https://github.com/KitwareMedical/SlicerCustomAppTemplate) project maintained by Kitware.
