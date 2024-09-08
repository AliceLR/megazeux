# Building MegaZeux with Xcode

## Instructions

The process of building MegaZeux using Xcode should be fairly straightforward and mostly painless. All required third-party frameworks are prebuilt and provided in the releases of this Github repository:

https://github.com/AliceLR/megazeux-dependencies

The following steps should produce a usable binary:

1. Fetch the latest frameworks tarball and extract it into arch/xcode/.
2. After cloning the repository, open the Xcode project contained in arch/xcode/.
3. Make sure that the desired build target (probably MegaZeux or MZXRun) is selected in the bar on the top of the screen.
4. Press Cmd+B to build the project or Cmd+R to build and run the project.

That will generate a debug build. To generate a release build, either build an archive (Product > Archive) and export the app bundle (recommended) or edit the Run scheme (Cmd+Shift+, or Product > Scheme > Edit Scheme...) and change the build configuration in the Info tab.

To change the build features, modify config.h. Please note that changing certain features, such as the module playback engine, will require additional files to be added to the project. The required modules can be found in the Makefile.in files in the src/ directory.

## Compatibility

At this time, MegaZeux has been successfully built using Xcode 9.1 on a system running macOS 10.13. The resulting binary has been tested on OS X 10.7 and macOS 10.13, though it should work on versions as low as OS X 10.6.

