// SPDX-FileCopyrightText: 2023 sudachi Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

import android.annotation.SuppressLint
import kotlin.collections.setOf
import org.jlleitschuh.gradle.ktlint.reporter.ReporterType
import com.github.triplet.gradle.androidpublisher.ReleaseStatus

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("kotlin-parcelize")
    kotlin("plugin.serialization") version "1.9.20"
    id("androidx.navigation.safeargs.kotlin")
    id("org.jlleitschuh.gradle.ktlint") version "11.4.0"
    id("com.github.triplet.play") version "3.8.6"
}

/**
 * Use the number of seconds/10 since Jan 1 2016 as the versionCode.
 * This lets us upload a new build at most every 10 seconds for the
 * next 680 years.
 */
val autoVersion = (((System.currentTimeMillis() / 1000) - 1451606400) / 10).toInt()

@Suppress("UnstableApiUsage")
android {
    namespace = "org.sudachi.sudachi_emu"
    compileSdk = 34

    ndkVersion = "26.2.11394342"

    buildFeatures {
        viewBinding = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        // This is necessary for libadrenotools custom driver loading
        jniLibs.useLegacyPackaging = true
    }

    androidResources {
        generateLocaleConfig = true
    }

    defaultConfig {
        // TODO If this is ever modified, change application_id in strings.xml
        applicationId = "org.sudachi.sudachi_emu"
        minSdk = 30
        targetSdk = 34
        versionName = getGitVersion()

        versionCode = if (System.getenv("AUTO_VERSIONED") == "true") {
            autoVersion
        } else {
            1
        }

        ndk {
            @SuppressLint("ChromeOsAbiSupport")
            abiFilters += listOf("arm64-v8a")
        }

        buildConfigField("String", "GIT_HASH", "\"${getGitHash()}\"")
        buildConfigField("String", "BRANCH", "\"${getBranch()}\"")
    }

    val keystoreFile = System.getenv("ANDROID_KEYSTORE_FILE")
    signingConfigs {
        if (keystoreFile != null) {
            create("release") {
                storeFile = file(keystoreFile)
                storePassword = System.getenv("ANDROID_KEYSTORE_PASS")
                keyAlias = System.getenv("ANDROID_KEY_ALIAS")
                keyPassword = System.getenv("ANDROID_KEYSTORE_PASS")
            }
        }
        create("default") {
            storeFile = file("$projectDir/debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }

    // Define build types, which are orthogonal to product flavors.
    buildTypes {

        // Signed by release key, allowing for upload to Play Store.
        release {
            signingConfig = if (keystoreFile != null) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("default")
            }

            resValue("string", "app_name_suffixed", "sudachi")
            isMinifyEnabled = true
            isDebuggable = false
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
        }

        // builds a release build that doesn't need signing
        // Attaches 'debug' suffix to version and package name, allowing installation alongside the release build.
        register("relWithDebInfo") {
            isDefault = true
            resValue("string", "app_name_suffixed", "sudachi Debug Release")
            signingConfig = signingConfigs.getByName("default")
            isMinifyEnabled = true
            isDebuggable = true
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
            versionNameSuffix = "-relWithDebInfo"
            applicationIdSuffix = ".relWithDebInfo"
            isJniDebuggable = true
        }

        // Signed by debug key disallowing distribution on Play Store.
        // Attaches 'debug' suffix to version and package name, allowing installation alongside the release build.
        debug {
            signingConfig = signingConfigs.getByName("default")
            resValue("string", "app_name_suffixed", "sudachi Debug")
            isDebuggable = true
            isJniDebuggable = true
            versionNameSuffix = "-debug"
            applicationIdSuffix = ".debug"
        }
    }

    flavorDimensions.add("version")
    productFlavors {
        create("mainline") {
            isDefault = true
            dimension = "version"
            buildConfigField("Boolean", "PREMIUM", "false")
        }

        create("ea") {
            dimension = "version"
            buildConfigField("Boolean", "PREMIUM", "true")
            applicationIdSuffix = ".ea"
        }
    }

    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path = file("../../../CMakeLists.txt")
        }
    }
    buildToolsVersion = "34.0.0"

    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments(
                    "-DENABLE_QT=0", // Don't use QT
                    "-DENABLE_SDL2=0", // Don't use SDL
                    "-DENABLE_WEB_SERVICE=0", // Don't use telemetry
                    "-DBUNDLE_SPEEX=ON",
                    "-DANDROID_ARM_NEON=true", // cryptopp requires Neon to work
                    "-DSUDACHI_USE_BUNDLED_VCPKG=ON",
                    "-DSUDACHI_USE_BUNDLED_FFMPEG=ON",
                    "-DSUDACHI_ENABLE_LTO=ON",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
                )

                abiFilters("arm64-v8a", "x86_64")
            }
        }
    }
}

tasks.create<Delete>("ktlintReset") {
    delete(File(layout.buildDirectory.toString() + File.separator + "intermediates/ktLint"))
}

val showFormatHelp = {
    logger.lifecycle(
        "If this check fails, please try running \"gradlew ktlintFormat\" for automatic " +
            "codestyle fixes"
    )
}
tasks.getByPath("ktlintKotlinScriptCheck").doFirst { showFormatHelp.invoke() }
tasks.getByPath("ktlintMainSourceSetCheck").doFirst { showFormatHelp.invoke() }
tasks.getByPath("loadKtlintReporters").dependsOn("ktlintReset")

ktlint {
    version.set("0.47.1")
    android.set(true)
    ignoreFailures.set(false)
    reporters {
        reporter(ReporterType.CHECKSTYLE)
    }
}

play {
    val keyPath = System.getenv("SERVICE_ACCOUNT_KEY_PATH")
    if (keyPath != null) {
        serviceAccountCredentials.set(File(keyPath))
    }
    track.set(System.getenv("STORE_TRACK") ?: "internal")
    releaseStatus.set(ReleaseStatus.COMPLETED)
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.fragment:fragment-ktx:1.6.2")
    implementation("androidx.documentfile:documentfile:1.0.1")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.7.0")
    implementation("io.coil-kt:coil:2.6.0")
    implementation("androidx.core:core-splashscreen:1.0.1")
    implementation("androidx.window:window:1.2.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.swiperefreshlayout:swiperefreshlayout:1.1.0")
    implementation("androidx.navigation:navigation-fragment-ktx:2.7.7")
    implementation("androidx.navigation:navigation-ui-ktx:2.7.7")
    implementation("info.debatty:java-string-similarity:2.0.0")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.6.3")
}

fun runGitCommand(command: List<String>): String {
    return try {
        ProcessBuilder(command)
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start().inputStream.bufferedReader().use { it.readText() }
            .trim()
    } catch (e: Exception) {
        logger.error("Cannot find git")
        ""
    }
}

fun getGitVersion(): String {
    val gitVersion = runGitCommand(
        listOf(
            "git",
            "describe",
            "--always",
            "--long"
        )
    ).replace(Regex("(-0)?-[^-]+$"), "")
    val versionName = if (System.getenv("GITHUB_ACTIONS") != null) {
        System.getenv("GIT_TAG_NAME") ?: gitVersion
    } else {
        gitVersion
    }
    return versionName.ifEmpty { "0.0" }
}

fun getGitHash(): String =
    runGitCommand(listOf("git", "rev-parse", "--short", "HEAD")).ifEmpty { "dummy-hash" }

fun getBranch(): String =
    runGitCommand(listOf("git", "rev-parse", "--abbrev-ref", "HEAD")).ifEmpty { "dummy-hash" }
