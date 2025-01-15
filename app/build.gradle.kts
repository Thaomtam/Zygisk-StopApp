plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "es.thoitiet.KTAify"
    compileSdk = 35
    buildToolsVersion = "35.0.0"
    ndkVersion = "27.1.12297006"

    buildFeatures {
        prefab = true
    }

    packaging {
        resources {
            excludes += "**"
        }
    }

    defaultConfig {
        applicationId = "es.thoitiet.KTAify"
        minSdk = 26
        targetSdk = 35
        versionCode = 17700
        versionName = "v17.7"
        multiDexEnabled = false

        externalNativeBuild {
            cmake {
                abiFilters(
                    "arm64-v8a",
                    "armeabi-v7a"
                )

                arguments(
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DANDROID_STL=none",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
                )

                cFlags(
                    "-std=gnu23",
                    "-fvisibility=hidden",
                    "-fvisibility-inlines-hidden"
                )

                cppFlags(
                    "-std=gnu++20",
                    "-exceptions",
                    "-fno-rtti",
                    "-fvisibility=hidden",
                    "-fvisibility-inlines-hidden"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            multiDexEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
}

dependencies {
    implementation(libs.cxx)
    implementation(libs.hiddenapibypass)
}

// --------------------------------------------------
// Tác vụ cập nhật versionName/versionCode vào module.prop
// --------------------------------------------------
tasks.register("updateModuleProp") {
    doLast {
        val versionName = project.android.defaultConfig.versionName
        val versionCode = project.android.defaultConfig.versionCode

        val modulePropFile = project.rootDir.resolve("module/module.prop")

        var content = modulePropFile.readText()

        content = content.replace(Regex("version=.*"), "version=$versionName")
        content = content.replace(Regex("versionCode=.*"), "versionCode=$versionCode")

        modulePropFile.writeText(content)
    }
}

// --------------------------------------------------
// Tác vụ copy file classes.dex và native .so vào thư mục module
// --------------------------------------------------
tasks.register("copyFiles") {
    dependsOn("updateModuleProp")

    doLast {
        val moduleFolder = project.rootDir.resolve("module")

        // Đường dẫn đến file .dex (sau khi R8/Proguard chạy)
        val dexFile =
            project.layout.buildDirectory.get()
                .asFile
                .resolve("intermediates/dex/release/minifyReleaseWithR8/classes.dex")

        // Đường dẫn đến thư mục .so đã được strip
        val soDir =
            project.layout.buildDirectory.get()
                .asFile
                .resolve("intermediates/stripped_native_libs/release/stripReleaseDebugSymbols/out/lib")

        // Copy file DEX
        dexFile.copyTo(moduleFolder.resolve("classes.dex"), overwrite = true)

        // Copy các file .so theo ABI
        soDir.walk().filter { it.isFile && it.extension == "so" }.forEach { soFile ->
            val abiFolder = soFile.parentFile.name
            val destination = moduleFolder.resolve("zygisk/$abiFolder.so")
            soFile.copyTo(destination, overwrite = true)
        }
    }
}

// --------------------------------------------------
// Tác vụ đóng gói module thành 1 file ZIP
// --------------------------------------------------
tasks.register<Zip>("zip") {
    dependsOn("copyFiles")

    // Đổi tên file ZIP cho phù hợp với module KTAify
    archiveFileName.set("KTAify_${project.android.defaultConfig.versionName}.zip")
    destinationDirectory.set(project.rootDir.resolve("out"))

    from(project.rootDir.resolve("module"))
}

// --------------------------------------------------
// Sau khi build xong (assembleRelease), ta chạy cập nhật prop + copy file + zip
// --------------------------------------------------
afterEvaluate {
    tasks["assembleRelease"].finalizedBy("updateModuleProp", "copyFiles", "zip")
}
