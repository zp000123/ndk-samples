/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// ============================================================
// 【头文件包含】
// ============================================================

// 包含项目基础工具宏定义，其中定义了 arraysize() 宏用于计算数组元素个数
#include <base/macros.h>

// 【JNI 核心头文件】
// jni.h 是 JNI (Java Native Interface) 的核心头文件
// 定义了所有 JNI 相关的类型、函数和宏，如 JNIEnv、JavaVM、JNI_OnLoad 等
#include <jni.h>

// 【C++ 标准库】
// string 是 C++ 标准字符串类，提供了方便的字符串操作
#include <string>

// ============================================================
// 【本地函数实现】
// ============================================================

/**
 * 【本地方法实现】
 * 这个函数对应 Java 代码中的 native 方法 stringFromJNI()
 * 
 * @param env - JNI 接口指针，是访问 JNI 函数的入口
 * @param thiz - 调用此方法的 Java 对象实例（this 指针），这里用 jobject 占位
 * @return jstring - 返回 Java 字符串对象
 * 
 * 【JNI 类型说明】
 * - JNIEnv*: 指向 JNI 函数表的指针，所有 JNI 操作都通过它进行
 * - jobject: 代表 Java 对象引用
 * - jstring: 代表 Java String 对象的本地引用
 */
jstring StringFromJni(JNIEnv* env, jobject) {
  // 【创建 C++ 字符串】
  // 使用 C++ std::string 类创建字符串，比 C 风格字符串更安全
  std::string hello = "Hello from JNI.";
  
  // 【转换为 Java 字符串】
  // env->NewStringUTF() 是 JNI 函数，将 C/C++ UTF-8 字符串转换为 Java String 对象
  // c_str() 返回 C 风格字符串指针（const char*）
  // 注意：返回的 jstring 是局部引用，会自动被 JVM 管理
  return env->NewStringUTF(hello.c_str());
}

// ============================================================
// 【JNI 库加载入口】
// ============================================================

/**
 * 【JNI_OnLoad - 库加载回调函数】
 * 当 System.loadLibrary() 加载 native 库时，JVM 会自动调用此函数
 * 这是注册本地方法的最佳时机，也是初始化 native 库的地方
 * 
 * 【为什么使用 JNI_OnLoad 而不是自动命名匹配？】
 * 1. 性能更好：避免运行时按名称查找函数的开销
 * 2. 更安全：编译期就能发现问题
 * 3. 更灵活：Java 方法名和 C++ 函数名可以不一致
 * 4. 支持混淆：Java 代码混淆后仍然能正常工作
 * 
 * @param vm - JavaVM 指针，代表整个 JVM 实例
 * @param reserved - 保留参数，当前未使用
 * @return jint - 返回 JNI 版本号，告诉 JVM 这个库需要什么 JNI 版本
 */
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* _Nonnull vm, void* _Nullable) {
  // 【获取 JNI 环境指针】
  // JNIEnv 是线程本地的，每个线程都有自己的 JNIEnv
  // GetEnv() 获取当前线程的 JNIEnv 指针
  JNIEnv* env;
  
  // 【检查 JNI 版本兼容性】
  // JNI_VERSION_1_6 表示需要 JNI 1.6 版本支持
  // 如果 JVM 不支持这个版本，返回 JNI_ERR 表示加载失败
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    // 【JNI_ERR 在 Java 层的表现】
    // 返回 JNI_ERR 会导致 JVM 抛出 UnsatisfiedLinkError
    // 这是 Error 类型，不是 Exception，无法通过 try-catch 捕获！
    // 
    // 【Java 层现象】
    // System.loadLibrary("hello-jni") 会抛出：
    // java.lang.UnsatisfiedLinkError: JNI_ERR returned from JNI_OnLoad
    //
    // 【为什么捕获不住？】
    // UnsatisfiedLinkError extends LinkageError extends Error
    // Error 是严重错误，应用程序通常不应该捕获
    // 即使写了 try-catch (Exception e)，也捕获不到 Error
    //
    // 【正确处理】
    // 如果真的要处理，必须 catch (Throwable t) 或 catch (Error e)
    // 但通常建议让程序崩溃，因为 native 库加载失败是致命错误
    return JNI_ERR;
  }

  // 【查找 Java 类】
  // FindClass 通过完整类名（包名/类名格式）查找 Java 类
  // 注意：使用 / 而不是 . 作为包分隔符
  // 返回 jclass 是局部引用，在本函数返回后会被自动释放
  jclass c = env->FindClass("com/example/hellojni/HelloJni");
  
  // 【错误处理】
  // 如果类找不到（比如包名写错），返回 nullptr
  // 此时应该返回错误，否则后续 RegisterNatives 会崩溃
  if (c == nullptr) return JNI_ERR;

  // 【定义本地方法映射表】
  // JNINativeMethod 结构体定义了 Java 方法名、签名和对应的 C++ 函数指针
  // 这是 "RegisterNatives" 方式注册本地方法的核心
  static const JNINativeMethod methods[] = {
      {
        // 【Java 方法名】
        // 必须与 Java 代码中声明的 native 方法名完全一致
        "stringFromJNI",
        
        // 【方法签名】
        // JNI 使用特殊的字符串表示方法签名
        // "()Ljava/lang/String;" 表示：
        //   () - 无参数
        //   Ljava/lang/String; - 返回类型是 String 对象
        // 其他常见签名：
        //   I = int, J = long, Z = boolean, F = float, D = double
        //   [I = int[], [[I = int[][], V = void
        "()Ljava/lang/String;",
        
        // 【C++ 函数指针】
        // reinterpret_cast 将函数指针转换为 void* 类型
        // 这是 C++ 风格的类型转换，比 C 的强制转换更安全
        reinterpret_cast<void*>(StringFromJni)
      },
  };

  // 【注册本地方法】
  // RegisterNatives 将 C++ 函数与 Java native 方法绑定
  // 参数说明：
  //   c - 要注册方法的 Java 类
  //   methods - 方法映射表数组
  //   arraysize(methods) - 数组元素个数（使用 base/macros.h 中的宏）
  // 返回值：JNI_OK 表示成功，其他值表示失败
  int rc = env->RegisterNatives(c, methods, arraysize(methods));
  
  // 【检查注册结果】
  // 如果注册失败（比如方法签名写错），返回错误码
  if (rc != JNI_OK) return rc;

  // 【返回 JNI 版本】
  // 告诉 JVM 这个 native 库需要什么 JNI 版本
  // 返回的版本号决定了 JVM 提供的 JNI 功能级别
  //
  // 【JNI 版本与 Android 版本对应关系】
  // ┌─────────────────┬─────────────────┬─────────────────────────────────────┐
  // │ JNI 版本         │ Android API 级别 │ 说明                                 │
  // ├─────────────────┼─────────────────┼─────────────────────────────────────┤
  // │ JNI_VERSION_1_1 │ API 1+ (1.0)    │ 基础 JNI 功能，已废弃不推荐           │
  // │ JNI_VERSION_1_2 │ API 1+ (1.0)    │ 添加了 MonitorEnter/Exit 等          │
  // │ JNI_VERSION_1_4 │ API 1+ (1.0)    │ 添加了 NewDirectByteBuffer 等        │
  // │ JNI_VERSION_1_6 │ API 4+ (1.6)    │ 推荐版本，添加了更多反射功能          │
  // └─────────────────┴─────────────────┴─────────────────────────────────────┘
  //
  // 【如何选择版本？】
  // - 现代 Android 开发统一使用 JNI_VERSION_1_6
  // - 它兼容 Android 1.6 (API 4) 及以上版本
  // - 覆盖目前 99.9%+ 的活跃设备
  //
  // 【版本不匹配会怎样？】
  // - 如果返回的版本 JVM 不支持，会抛出 UnsatisfiedLinkError
  // - 但 JNI 1.6 从 Android 1.6 就支持了，几乎不会遇到此问题
  return JNI_VERSION_1_6;
}
