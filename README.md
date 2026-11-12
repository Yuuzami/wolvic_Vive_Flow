# Wolvic for HTC VIVE Flow

Unofficial fork of [Wolvic](https://github.com/Igalia/wolvic) ([wolvic.com](https://www.wolvic.com)) that builds and runs on the **HTC VIVE Flow**, an Android 9 (API 28) headset that uses the VIVE **Wave SDK**. It is a personal-use port and is not affiliated with or supported by Igalia or HTC.

- **Target device:** HTC VIVE Flow only. Other headsets are not tested in this fork, even though the upstream build flavors are still in the tree.
- **Platform flavor:** `wavevr` (package `com.igalia.wolvic.internal`) or `wavevrStore` (package `com.igalia.wolvic`).
- **Wave SDK:** 5.6.0. The Flow supports Wave SDK up to 6.0.0, because later versions raise the minimum Android API level above 28.
- **Browser engine:** GeckoView (prebuilt nightly from Mozilla's maven repository). The Chromium backend is not used.
- **ABI:** `arm64` only.
- **SDK levels:** `minSdk` 26, `targetSdk` 31 for the Wave flavors.
- **Upstream:** `Igalia/wolvic`. This fork is not intended to be contributed back, because upstream no longer supports the VIVE Flow. Base branch of this port: `flow-port`.

## Building for the VIVE Flow

### Prerequisites

- Git, with **Git Bash** installed. On Windows, Git's `bin` folder must come before `C:\Windows\System32` on `PATH`, otherwise CMake picks up the WSL `bash.exe` launcher and the KTX-Software step fails.
- Android SDK with `ANDROID_HOME` set (or `sdk.dir` in `local.properties`) and the SDK licenses accepted. Gradle downloads the Android platform, build-tools, NDK and CMake it needs.
- JDK 21. Gradle provisions it automatically through the daemon toolchain settings (`gradle/gradle-daemon-jvm.properties`), which needs network access on the first build.
- Network access to `maven.mozilla.org` for GeckoView and the Android Components.
- The **VIVE Wave SDK 5.6.0 (native)** from the [VIVE Developer Resources archive](https://developer.vive.com/resources/vive-wave/download/archive/5_6_0/). It is proprietary and is not part of this repository.

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/Yuuzami/wolvic_Vive_Flow.git wolvic
git -C wolvic checkout flow-port
```

The build fails without the submodules (`vrb`, `tinygltf`, `KTX-Software`).

### 2. Place the Wave SDK next to the repository

The build reads the SDK from `../third_party/wave_5.6.0_native`, outside the repository. Unzip it so that this file exists:

```
<parent folder>/
  wolvic/                                  <- this repository
  third_party/
    wave_5.6.0_native/
      repo/com/htc/vr/wvr_client/5.6.0/wvr_client-5.6.0.aar
```

The `.aar` is unpacked automatically, but only when the first Gradle task name contains `wavevr`, so always build through a `wavevr` task.

### 3. Build

From the repository root, in PowerShell with Git Bash first on `PATH`:

```powershell
$env:PATH = "$env:LOCALAPPDATA\Programs\Git\bin;$env:PATH"
.\gradlew.bat :app:assembleWavevrArm64GeckoGenericDebug
```

If you hit the WSL `bash.exe` error after a failed attempt, delete `app/.cxx` and build again.

Output: `app/build/outputs/apk/wavevrArm64GeckoGeneric/debug/Wolvic-wavevr-arm64-gecko-generic-debug.apk`

### 4. Install on the Flow

With developer mode and USB debugging enabled on the headset:

```bash
adb install -r app/build/outputs/apk/wavevrArm64GeckoGeneric/debug/Wolvic-wavevr-arm64-gecko-generic-debug.apk
adb shell am start -n com.igalia.wolvic.internal/com.igalia.wolvic.VRBrowserActivity
```

Add `-e homepage "https://example.com"` to the `am start` command to load a specific homepage.

### Release build

```powershell
.\gradlew.bat :app:assembleWavevrArm64GeckoGenericRelease
```

The release APK is unsigned (and not installable) unless a keystore is configured. Create a `keystore.properties` in the repository root (git-ignored) with `storeFile` (relative to the repository root, or absolute), `storePassword`, `keyAlias` and `keyPassword`. Updates must be signed with the same key as the installed version, so keep the keystore safe.

## Build options

Optional properties for `user.properties` in the repository root:

- `simultaneousDevProduction=true`: adds a dev applicationID suffix so dev and production builds can be installed side by side.
- `useDebugSigningOnRelease=true`: signs release builds with the debug keystore for local testing. Not for distribution.
- `useStaticVersionCode=true`: keeps the version code constant between builds, avoiding needless Gradle task re-runs.

## Upstream developer notes

The rest of this file is the upstream Wolvic developer documentation. The Flow build above does not need any of it, apart from the build options (`user.properties`) further down.

### GeckoView local substitution (optional)

After [PR #70](https://github.com/Igalia/wolvic/pull/70), WebXR sessions won't work with the prebuilt maven GeckoView libraries because that PR introduced a change in the GeckoView protocol. So you have to build GeckoView manually by applying patches at [this repository](https://github.com/Igalia/wolvic-gecko-patches).

This could be done either by using a [local maven repository](http://mozac.org/contributing/testing-components-inside-app) (quite cumbersome), or via Gradle's [dependency substitutions](https://docs.gradle.org/current/userguide/dependency_resolution.html) (not at all cumbersome!).

Currently, the substitution flow is streamlined for some of the core dependencies via configuration flags in `local.properties`. You can build against a local checkout of the following dependencies by specifying their local paths:
- [GeckoView](https://hg.mozilla.org/releases/mozilla-release/tags), specifying its path via `dependencySubstitutions.geckoviewTopsrcdir=/path/to/mozilla-release` (and, optionally, `dependencySubstitutions.geckoviewTopobjdir=/path/to/topobjdir`). See [Bug 1533465](https://bugzilla.mozilla.org/show_bug.cgi?id=1533465).
  - This assumes that you have built, packaged, and published your local GeckoView -- but don't worry, the dependency substitution script has the latest instructions for doing that.
  - If you want to build for different architectures at the same time, you can specify the aarch64 path via `dependencySubstitutions.geckoviewTopobjdir` while the x86_64 path via `dependencySubstitutions.geckoviewTopobjdirX64`

Do not forget to run a Gradle sync in Android Studio after changing `local.properties`. If you specified any substitutions, they will be reflected in the modules list, and you'll be able to modify them from a single Android Studio window.

For step by step guide check [here](https://github.com/Igalia/wolvic/wiki/Developer-workflow#building-gecko).

### Android Studio

Open the repository root in [Android Studio](https://developer.android.com/studio/index.html), then go to `Tool Windows > Build Variants` and select `wavevrArm64GeckoGenericDebug`. The Wave SDK placement and Git Bash requirements above still apply.

## Local Development

> By default the build downloads prebuilt GeckoView libraries from [Mozilla's maven repositories](https://maven.mozilla.org/maven2/org/mozilla/geckoview/?prefix=maven2/org/mozilla/geckoview/). This is what the Flow build uses. Skip the *GeckoView local substitution* section above unless you need a custom GeckoView.

## Install dev and production builds on device simultaneously

You can enable a dev applicationID sufix to install both dev and production builds simultaneously. You just need to add this property to your `user.properties` file:

```ini
simultaneousDevProduction=true
```
## Locally generate Android release builds

Local release builds can be useful to measure performance or debug issues only happening in release builds. Insead of dealing with release keys you can make the testing easier just adding this property to your `user.properties` file:

```ini
useDebugSigningOnRelease=true
```

Note: the release APKs generated with a debug keystore can't be used for production.

## Generate builds with a static version code

By default, each build will be assigned an auto-generated version code, which is derived from the date when the build was created. This behavior interferes with Gradle's caching mechanism and unnecessarily re-runs tasks that depend on the version code, leading to longer build times. You can work around this by temporarily using a static version code that persists between builds. This can be done by setting this property in your `user.properties` file:

```ini
useStaticVersionCode=true
```

## Compress assets

ETC2 compression is used to improve performance and memory usage. Raw assets are placed in the `uncompressed_assets` folder. You can generate the compressed textures using the compressor utility in `tools/compressor`. You need to set up [etc2comp](https://github.com/google/etc2comp) and make it available on your PATH before running the script. Run this command to generate the compressed assets:

```bash
cd tools/compressor
npm install
npm run compress
```

## Locale support

For more info on localization, how it works in the Wolvic XR project, and how to correctly edit localizable text in the application, please see our [localization wiki page](https://github.com/Igalia/wolvic/wiki/Localization).

## Development troubleshooting

### `Device supports , but APK only supports armeabi-v7a[...]`

Enable [USB Remote Debugging](https://github.com/MozillaReality/FirefoxReality/wiki/Developer-Info#remote-debugging) on the device.

### **`Firefox > Web Developer > WebIDE > Performance`** gets stuck with greyed out "stop and show profile"

Restart Wolvic XR and close and re-open the WebIDE page.

### **`Tool Windows > Build Variants`** list is empty

1. If you're not on the latest version, update Android Studio from **`Android Studio > Check for Updates…`**.
2. Run **`File > Sync Project with Gradle Files`**.

## Debugging tips

- When using the native debugger you can ignore the first SIGSEGV: address access protected stop in GV thread. It's not a crash; you can click *Resume* to continue debugging.
- On some platforms such as Oculus Go the native debugger stops on each input event. You can set this LLDB post-attach command in Android Studio to fix the problem: `pro hand -p true -s false SIGILL`
- You can use `adb shell am start -a android.intent.action.VIEW -d "https://aframe.io" com.igalia.wolvic/com.igalia.wolvic.VRBrowserActivity` to load a URL from the command line
- You can use `adb shell am start -a android.intent.action.VIEW  -n com.igalia.wolvic/com.igalia.wolvic.VRBrowserActivity -e homepage "https://example.com"` to override the homepage
- You can use `adb shell setprop debug.oculus.enableVideoCapture 1` to record a video on the Oculus Go. Remember to run `adb shell setprop debug.oculus.enableVideoCapture 0` to stop recording the video.
    - You can also record videos on the Oculus Go by exiting to the system library, and from the Oculus tray menu (toggle with the Oculus button on the controller): **`Sharing > Record Video`**
- You can set `disableCrashRestart=true` in the gradle `user.properties` to disable app relaunch on crash.