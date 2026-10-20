import java.util.Properties

plugins {
    id("com.android.application")
}

android {
    // Java sources keep Mike Reid's original fm.a2d.sf package; only the installed app id is ours.
    namespace = "fm.a2d.sf"
    compileSdk = 36

    defaultConfig {
        applicationId = "dev.dreamz.spiritfm"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "3.0.0-mtk1"
        ndk { abiFilters += setOf("arm64-v8a") }
        externalNativeBuild { ndkBuild { arguments += listOf("APP_PLATFORM=android-29") } }
    }

    externalNativeBuild {
        ndkBuild { path = file("src/main/jni/Android.mk") }
    }
    ndkVersion = "29.0.14206865"

    // s2d is a root daemon shipped as libs2d.so; it must be extracted to disk so su can exec it.
    packaging { jniLibs { useLegacyPackaging = true } }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    lint { abortOnError = false }
}

// Build the s2d root daemon as a PIE executable named libs2d.so so it ships in lib/arm64-v8a and gets extracted to disk.
val s2dOutDir = layout.buildDirectory.dir("generated/s2d/jniLibs")
val sdkDir = Properties().apply { rootProject.file("local.properties").inputStream().use { load(it) } }
    .getProperty("sdk.dir") ?: System.getenv("ANDROID_HOME")
val buildS2d = tasks.register<Exec>("buildS2d") {
    val ndkDir = file("$sdkDir/ndk/${android.ndkVersion}")
    val clang = ndkDir.resolve("toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang")
    val jni = file("src/main/jni")
    val out = s2dOutDir.get().file("arm64-v8a/libs2d.so").asFile
    inputs.dir(jni)
    outputs.file(out)
    doFirst { out.parentFile.mkdirs() }
    commandLine(
        clang.absolutePath, "-O2", "-fPIE", "-pie", "-Wno-error=implicit-function-declaration", "-Wno-int-conversion", "-Wno-incompatible-pointer-types",
        "-I", jni.absolutePath,
        jni.resolve("s2d/s2d.c").absolutePath,
        "-llog", "-ldl", "-o", out.absolutePath,
    )
}
android.sourceSets.getByName("main").jniLibs.directories.add(s2dOutDir.get().asFile.absolutePath)
tasks.named("preBuild") { dependsOn(buildS2d) }
