# Building for Android
Documentation on how to build Sudachi for Android

## Requirements
> [!NOTE]
> Below is a list of development tools, software, etc. required for building Sudachi for Android

### Development Tools
- Git
- Java Development Kit
- Vulkan® SDK
    - Used: 1.3.280.0

### Software
- Android Studio
    - Used: Iguana
- Visual Studio (Windows Only)
    - Used: 17.9.6
    - Workloads
        - Desktop development with C++

## Steps
### Step I - Cloning
```sh
cd path/to/directory
git clone --recursive https://github.com/sudachi-emu/sudachi
```

### Step II - Building
```sh
cd src/android
gradlew.bat build # gradlew if building on a Unix system
```