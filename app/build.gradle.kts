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
