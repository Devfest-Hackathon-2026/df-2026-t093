plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("com.android.library")
}

kotlin {
    compilerOptions { freeCompilerArgs.add("-Xexpect-actual-classes") }
    androidTarget { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }
    listOf(iosArm64(), iosSimulatorArm64()).forEach {
        it.binaries.framework { baseName = "Shared"; isStatic = true }
    }
    sourceSets {
        commonMain.dependencies { implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2") }
        commonTest.dependencies { implementation(kotlin("test")) }
    }
}

android {
    namespace = "dev.lifeos.pendant.shared"
    compileSdk = 36
    defaultConfig { minSdk = 31 }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
}

// verify.sh runs the C interop tests as their own step: -PinteropOnly runs only them, otherwise they are left out.
tasks.withType<Test>().configureEach {
    if (project.hasProperty("interopOnly")) filter.includeTestsMatching("*InteropTest") else filter.excludeTestsMatching("*InteropTest")
    testLogging { events("failed"); exceptionFormat = org.gradle.api.tasks.testing.logging.TestExceptionFormat.FULL }
}
