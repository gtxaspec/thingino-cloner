# Thingino Cloner proguard rules
# Keep JNI methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep the ClonerBridge callback interface
-keep class com.thingino.cloner.ClonerBridge$NativeCallback { *; }
-keep class com.thingino.cloner.ClonerBridge { *; }
