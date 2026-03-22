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

// 【Android 日志库】
// 提供 __android_log_print 函数，用于输出日志到 logcat
// 类似 Java 的 android.util.Log
#include <android/log.h>

// 【断言库】
// 提供 assert() 宏，用于调试时检查条件
#include <assert.h>

// 【项目基础库】
// 提供 arraysize() 等工具宏
#include <base/macros.h>

// 【整数类型格式化】
// 提供 PRId64 等宏，用于跨平台的 64 位整数格式化
// 例如：printf("%" PRId64, value); 输出 int64_t 类型
#include <inttypes.h>

// 【JNI 核心头文件】
// 提供 JNI 所有类型定义和函数声明
#include <jni.h>

// 【POSIX 线程库】
// 提供 pthread 相关函数，用于创建和管理线程
// 这是 Native 层实现多线程的标准方式
#include <pthread.h>

// 【字符串操作】
// 提供 memset、strcmp 等字符串/内存操作函数
#include <string.h>

// ============================================================
// 【日志宏定义】
// ============================================================

// 【日志标签】
// 所有日志都用这个标签，方便在 logcat 中过滤
// 使用命令：adb logcat -s hello-jniCallback:D
static const char* kTAG = "hello-jniCallback";

// 【LOGI - Info 级别日志】
// __android_log_print 是 NDK 提供的日志函数
// ANDROID_LOG_INFO 表示 INFO 级别
// __VA_ARGS__ 是变参宏，传递所有参数
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))

// 【LOGW - Warning 级别日志】
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))

// 【LOGE - Error 级别日志】
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// ============================================================
// 【全局上下文结构体】
// ============================================================

/**
 * 【TickContext - 线程上下文结构体】
 * 
 * 这个结构体保存了从 Java 层传递到 Native 线程的所有必要信息
 * 因为 pthread 创建的线程无法直接访问 JNIEnv，需要通过 JavaVM 获取
 * 
 * 【为什么需要这个结构体？】
 * 1. pthread 创建的线程没有 JNIEnv，需要通过 JavaVM->AttachCurrentThread 获取
 * 2. 需要保存 Java 对象的引用，才能在 Native 线程中回调 Java 方法
 * 3. 需要线程同步机制（mutex）来控制线程生命周期
 */
typedef struct tick_context {
  // 【JavaVM 指针】
  // 代表整个 JVM 实例，是线程无关的（所有线程共享）
  // 用于 AttachCurrentThread/DetachCurrentThread
  JavaVM* javaVM;
  
  // 【JniHandler 类的全局引用】
  // jclass 是 JNI 中的类引用类型
  // 使用 GlobalRef 是因为要在多个线程中长期保存
  jclass jniHandlerClz;
  
  // 【JniHandler 实例的全局引用】
  // jobject 是 JNI 中的对象引用类型
  // 用于调用 JniHandler 的非静态方法
  jobject jniHandlerObj;
  
  // 【MainActivity 类的全局引用】
  // 用于调用 MainActivity 的方法（如 updateTimer）
  jclass mainActivityClz;
  
  // 【MainActivity 实例的全局引用】
  // 保存 MainActivity 的实例，用于更新 UI
  jobject mainActivityObj;
  
  // 【互斥锁】
  // pthread_mutex_t 是 POSIX 线程的互斥锁
  // 用于线程安全的访问 done 标志位
  pthread_mutex_t lock;
  
  // 【线程结束标志】
  // 0 = 继续运行，1 = 请求停止
  // 使用 volatile 会更好（虽然这里用了 mutex 保护）
  int done;
} TickContext;

// 【全局上下文实例】
// g_ctx 是全局变量，所有函数都可以访问
// 保存了 JNI 回调所需的所有 Java 对象引用
TickContext g_ctx;

// ============================================================
// 【StringFromJni - 基础 JNI 方法】
// ============================================================

/**
 * 【StringFromJni - 返回带 ABI 信息的字符串】
 * 
 * 这个函数演示了：
 * 1. 如何在编译期检测目标 CPU 架构
 * 2. 如何返回 Java 字符串给调用者
 * 
 * 【预处理器宏说明】
 * C/C++ 编译器会根据目标架构自动定义这些宏：
 * - __arm__ : 32位 ARM
 * - __ARM_ARCH_7A__ : ARMv7-A 架构
 * - __ARM_NEON__ : 支持 NEON 指令集
 * - __ARM_PCS_VFP : 硬浮点 ABI
 * - __i386__ : 32位 x86
 * - __x86_64__ : 64位 x86
 * - __aarch64__ : 64位 ARM (arm64-v8a)
 * - __mips__ / __mips64__ : MIPS 架构
 * 
 * 【ABI 检测的实际应用】
 * 1. 根据架构选择不同的汇编代码
 * 2. 加载对应的第三方库（如 OpenCV 有多个 ABI 版本）
 * 3. 在日志中标识当前运行的架构
 */
jstring StringFromJni(JNIEnv* env, jobject) {
  // 【条件编译检测 CPU 架构】
  // 这些宏在编译时由编译器自动定义
  // 只会匹配当前编译目标的架构
  
#if defined(__arm__)
  // 【32位 ARM 分支】
  #if defined(__ARM_ARCH_7A__)
    // 【ARMv7-A 架构】
    // 现代 32位 ARM 设备（Cortex-A 系列）
    #if defined(__ARM_NEON__)
      // 【支持 NEON 指令集】
      // NEON 是 ARM 的 SIMD（单指令多数据）扩展
      // 用于加速多媒体处理和信号处理
      #if defined(__ARM_PCS_VFP)
        // 【硬浮点 ABI】
        // 函数参数和返回值使用浮点寄存器传递
        // 性能更好
        #define ABI "armeabi-v7a/NEON (hard-float)"
      #else
        #define ABI "armeabi-v7a/NEON"
      #endif
    #else
      // 【不支持 NEON】
      #if defined(__ARM_PCS_VFP)
        #define ABI "armeabi-v7a (hard-float)"
      #else
        #define ABI "armeabi-v7a"
      #endif
    #endif
  #else
    // 【旧版 ARM】
    // 很少见了，主要是非常老的设备
    #define ABI "armeabi"
  #endif
  
#elif defined(__i386__)
  // 【32位 x86】
  // 主要用于 Android 模拟器
  #define ABI "x86"
  
#elif defined(__x86_64__)
  // 【64位 x86】
  // 用于 64位模拟器或 Intel 平板
  #define ABI "x86_64"
  
#elif defined(__mips64)
  // 【64位 MIPS】
  // mips64el-* 工具链同时定义了 __mips__ 和 __mips64__
  #define ABI "mips64"
  
#elif defined(__mips__)
  // 【32位 MIPS】
  // 很少见的架构
  #define ABI "mips"
  
#elif defined(__aarch64__)
  // 【64位 ARM】
  // 现代 Android 设备的主流架构（arm64-v8a）
  // 性能最好，支持更大的内存寻址
  #define ABI "arm64-v8a"
  
#else
  // 【未知架构】
  // 编译器不支持或新的架构
  #define ABI "unknown"
#endif

  // 【返回带 ABI 信息的字符串】
  // 字符串拼接在编译期完成，运行期只是返回常量
  // 例如："Hello from JNI !  Compiled with ABI arm64-v8a."
  return env->NewStringUTF("Hello from JNI !  Compiled with ABI " ABI ".");
}

// ============================================================
// 【queryRuntimeInfo - 查询运行时信息】
// ============================================================

/**
 * 【queryRuntimeInfo - 演示如何从 Native 调用 Java 方法】
 * 
 * 这个函数演示了 JNI 回调 Java 的两种主要方式：
 * 1. 调用静态方法（不需要实例）
 * 2. 调用实例方法（需要对象实例）
 * 
 * 【JNI 方法签名快速参考】
 * ┌──────────┬─────────────────────────────────────┐
 * │ 类型      │ 签名                                 │
 * ├──────────┼─────────────────────────────────────┤
 * │ void     │ V                                    │
 * │ boolean  │ Z                                    │
 * │ int      │ I                                    │
 * │ long     │ J                                    │
 * │ String   │ Ljava/lang/String;                   │
 * │ Object   │ L完整类名;                            │
 * │ 数组      │ [类型                                │
 * └──────────┴─────────────────────────────────────┘
 * 
 * @param env - JNI 环境指针
 * @param instance - JniHandler 的实例对象
 */
void queryRuntimeInfo(JNIEnv* env, jobject instance) {
  // ============================================================
  // 【第一部分：调用静态方法】
  // ============================================================
  
  // 【获取静态方法 ID】
  // GetStaticMethodID 用于获取类的静态方法
  // 参数：
  //   1. jclass - 类引用
  //   2. 方法名 - "getBuildVersion"
  //   3. 方法签名 - "()Ljava/lang/String;"
  //      () 表示无参数
  //      Ljava/lang/String; 表示返回 String
  jmethodID versionFunc = env->GetStaticMethodID(
      g_ctx.jniHandlerClz, "getBuildVersion", "()Ljava/lang/String;");
  
  // 【错误处理】
  // 如果方法找不到（类名错误、方法名错误、签名错误），返回 nullptr
  if (!versionFunc) {
    LOGE("Failed to retrieve getBuildVersion() methodID @ line %d", __LINE__);
    return;
  }
  
  // 【调用静态方法】
  // CallStaticObjectMethod 调用返回 Object 的静态方法
  // 静态方法不需要实例，只需要类引用
  // 返回的是局部引用（LocalRef），需要管理生命周期
  jstring buildVersion = static_cast<jstring>(
      env->CallStaticObjectMethod(g_ctx.jniHandlerClz, versionFunc));
  
  // 【转换为 C 字符串】
  // GetStringUTFChars 将 Java String 转换为 C 的 char*
  // 第二个参数 isCopy：
  //   - NULL（或 JNI_FALSE）：不关心是否拷贝
  //   - 传入 jboolean*：返回 JNI_TRUE 表示拷贝了新内存，JNI_FALSE 表示直接指针
  const char* version = env->GetStringUTFChars(buildVersion, NULL);
  if (!version) {
    LOGE("Unable to get version string @ line %d", __LINE__);
    return;
  }
  
  // 【输出日志】
  LOGI("Android Version - %s", version);
  
  // 【释放字符串】
  // 必须配对使用 GetStringUTFChars 和 ReleaseStringUTFChars
  // 否则会造成内存泄漏
  env->ReleaseStringUTFChars(buildVersion, version);

  // 【删除局部引用】
  // buildVersion 是局部引用，在函数返回后自动释放
  // 但这里显式删除是为了演示最佳实践
  // 如果创建了大量局部引用，必须及时删除，否则可能超出局部引用表限制（默认512个）
  env->DeleteLocalRef(buildVersion);

  // ============================================================
  // 【第二部分：调用实例方法】
  // ============================================================
  
  // 【获取实例方法 ID】
  // GetMethodID 用于获取实例方法（非静态）
  // "()J" 表示：无参数，返回 long（J 是 long 的 JNI 签名）
  jmethodID memFunc =
      env->GetMethodID(g_ctx.jniHandlerClz, "getRuntimeMemorySize", "()J");
  if (!memFunc) {
    LOGE("Failed to retrieve getRuntimeMemorySize() methodID @ line %d",
         __LINE__);
    return;
  }
  
  // 【调用实例方法】
  // CallLongMethod 调用返回 long 的实例方法
  // 第一个参数是对象实例（不是类！）
  jlong result = env->CallLongMethod(instance, memFunc);
  
  // 【输出日志】
  // PRId64 是跨平台的 64 位整数格式化宏
  // 在 Android 上，jlong 就是 int64_t
  LOGI("Runtime free memory size: %" PRId64, result);
  
  // 【消除编译器警告】
  // (void) 显式忽略返回值，告诉编译器"我是故意的"
  (void)result;
}

// ============================================================
// 【sendJavaMsg - 发送消息到 Java 层】
// ============================================================

/**
 * 【sendJavaMsg - 封装调用 Java 方法发送字符串消息】
 * 
 * 【JNI 可以调用 private 方法！】
 * JNI 不受 Java 访问控制修饰符限制（private/protected/public）
 * 这是 JNI 的强大之处，也是潜在的安全风险
 * 
 * 【使用场景】
 * - Native 线程需要向 Java 层报告状态
 * - 回调 Java 的监听器或处理器
 * - 将 Native 日志传递到 Java 层处理
 * 
 * @param env - JNI 环境指针
 * @param instance - Java 对象实例
 * @param func - 方法 ID（jmethodID）
 * @param msg - 要发送的 C 字符串消息
 */
void sendJavaMsg(JNIEnv* env, jobject instance, jmethodID func,
                 const char* msg) {
  // 【创建 Java 字符串】
  // 将 C 的 char* 转换为 Java 的 String 对象
  // 使用 UTF-8 编码
  jstring javaMsg = env->NewStringUTF(msg);
  
  // 【调用 Java 方法】
  // CallVoidMethod 调用返回 void 的实例方法
  // 参数：
  //   1. instance - 对象实例
  //   2. func - 方法 ID
  //   3. javaMsg - 方法参数（变参，可以有多个）
  env->CallVoidMethod(instance, func, javaMsg);
  
  // 【删除局部引用】
  // 及时释放，避免局部引用表溢出
  env->DeleteLocalRef(javaMsg);
}

// ============================================================
// 【UpdateTicks - Native 工作线程】
// ============================================================

/**
 * 【UpdateTicks - pthread 线程的主函数】
 * 
 * 【核心功能】
 * 这是一个 Native 创建的线程，每秒回调 Java 层更新 UI
 * 演示了 Native 线程如何安全地调用 Java 方法
 * 
 * 【线程模型说明】
 * Java 线程：由 JVM 管理，自动关联 JNIEnv
 * Native 线程：由 pthread_create 创建，需要手动 Attach 到 JVM
 * 
 * 【为什么需要 AttachCurrentThread？】
 * JNIEnv 是线程本地的（Thread-local）
 * Native 线程默认没有 JNIEnv，必须通过 AttachCurrentThread 获取
 * Attach 后，该线程就像 Java 线程一样，可以调用 JNI 函数
 * 
 * 【生命周期】
 * 1. AttachCurrentThread - 将线程关联到 JVM
 * 2. 执行 JNI 调用
 * 3. DetachCurrentThread - 解除关联（必须！否则内存泄漏）
 * 
 * @param context - 线程上下文（TickContext* 类型）
 * @return void* - 线程返回值
 */
void* UpdateTicks(void* context) {
  // 【解包上下文】
  TickContext* pctx = (TickContext*)context;
  JavaVM* javaVM = pctx->javaVM;
  JNIEnv* env;
  
  // 【获取或附加 JNIEnv】
  // GetEnv 尝试获取当前线程的 JNIEnv
  // 如果当前线程未 Attach，返回 JNI_EDETACHED
  jint res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
  if (res != JNI_OK) {
    // 【附加线程到 JVM】
    // AttachCurrentThread 将当前 Native 线程附加到 JVM
    // 附加后，该线程可以像 Java 线程一样使用 JNIEnv
    // 第二个参数是 JNIEnv 指针的指针，用于接收结果
    res = javaVM->AttachCurrentThread(&env, NULL);
    if (JNI_OK != res) {
      LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
      return NULL;
    }
  }

  // 【获取 JniHandler.updateStatus 方法 ID】
  // "(Ljava/lang/String;)V" 表示：接收 String 参数，返回 void
  jmethodID statusId = env->GetMethodID(pctx->jniHandlerClz, "updateStatus",
                                        "(Ljava/lang/String;)V");
  // 【发送初始化状态消息】
  sendJavaMsg(env, pctx->jniHandlerObj, statusId,
              "TickerThread status: initializing...");

  // 【获取 MainActivity.updateTimer 方法 ID】
  // "()V" 表示：无参数，返回 void
  jmethodID timerId =
      env->GetMethodID(pctx->mainActivityClz, "updateTimer", "()V");

  // 【时间变量定义】
  // struct timeval 用于存储秒和微秒
  // beginTime: 循环开始时间
  // curTime: 当前时间
  // usedTime: 本次循环已用时间
  // leftTime: 剩余时间（用于睡眠）
  struct timeval beginTime, curTime, usedTime, leftTime;
  
  // 【定义 1 秒的时间常量】
  // __kernel_time_t 和 __kernel_suseconds_t 是内核时间类型
  const struct timeval kOneSecond = {(__kernel_time_t)1,
                                     (__kernel_suseconds_t)0};

  // 【发送启动消息】
  sendJavaMsg(env, pctx->jniHandlerObj, statusId,
              "TickerThread status: start ticking ...");
  
  // 【主循环】
  // 每秒执行一次，直到 done 标志被设置
  while (1) {
    // 【记录循环开始时间】
    gettimeofday(&beginTime, NULL);
    
    // 【线程安全地读取 done 标志】
    // 使用互斥锁保护共享变量
    pthread_mutex_lock(&pctx->lock);
    int done = pctx->done;
    if (pctx->done) {
      // 重置标志，表示已处理停止请求
      pctx->done = 0;
    }
    pthread_mutex_unlock(&pctx->lock);
    
    // 【检查是否需要退出】
    if (done) {
      break;
    }
    
    // 【回调 Java 更新计时器】
    // 这会触发 MainActivity.updateTimer()，更新 UI 上的时间显示
    env->CallVoidMethod(pctx->mainActivityObj, timerId);

    // 【计算已用时间和剩余时间】
    gettimeofday(&curTime, NULL);
    // timersub 是 timeval 的减法：curTime - beginTime = usedTime
    timersub(&curTime, &beginTime, &usedTime);
    // 计算距离 1 秒还剩多少时间
    timersub(&kOneSecond, &usedTime, &leftTime);
    
    // 【准备睡眠】
    // struct timespec 用于 nanosleep，需要秒和纳秒
    struct timespec sleepTime;
    sleepTime.tv_sec = leftTime.tv_sec;
    // 微秒转纳秒：乘以 1000
    sleepTime.tv_nsec = leftTime.tv_usec * 1000;

    // 【睡眠或报错】
    // 如果剩余时间小于 1 秒，正常睡眠
    // 如果超过 1 秒，说明处理时间太长（异常情况）
    if (sleepTime.tv_sec <= 1) {
      nanosleep(&sleepTime, NULL);
    } else {
      sendJavaMsg(env, pctx->jniHandlerObj, statusId,
                  "TickerThread error: processing too long!");
    }
  }

  // 【发送停止消息】
  sendJavaMsg(env, pctx->jniHandlerObj, statusId,
              "TickerThread status: ticking stopped");
  
  // 【分离线程】
  // 必须调用 DetachCurrentThread，否则 JVM 无法回收线程相关资源
  // 会导致内存泄漏！
  javaVM->DetachCurrentThread();
  
  return context;
}

// ============================================================
// 【StartTicks - 启动计时器线程】
// ============================================================

/**
 * 【StartTicks - Java 调用的 Native 方法，启动后台线程】
 * 
 * 【调用时机】
 * 从 MainActivity.onResume() 调用，当 Activity 进入前台时启动计时
 * 
 * 【功能】
 * 1. 初始化线程属性（分离状态）
 * 2. 初始化互斥锁
 * 3. 创建 MainActivity 的全局引用（供 Native 线程使用）
 * 4. 创建 pthread 线程
 * 
 * 【全局引用 vs 局部引用】
 * 局部引用（LocalRef）：
 * - 只在当前 JNI 调用期间有效
 * - 函数返回后自动释放
 * - 不能跨线程使用
 * 
 * 全局引用（GlobalRef）：
 * - 直到显式删除前一直有效
 * - 可以跨线程使用
 * - 必须配对使用 NewGlobalRef 和 DeleteGlobalRef
 * 
 * 【为什么需要 GlobalRef？】
 * StartTicks 在 Java UI 线程执行，但 UpdateTicks 在 Native 线程执行
 * Native 线程需要访问 MainActivity 实例，必须使用 GlobalRef
 * 
 * @param env - JNI 环境指针
 * @param instance - MainActivity 实例（this 指针）
 */
void StartTicks(JNIEnv* env, jobject instance) {
  // 【线程 ID 和属性】
  pthread_t threadInfo_;      // 存储新线程的 ID
  pthread_attr_t threadAttr_; // 线程属性对象

  // 【初始化线程属性】
  // 设置线程为"分离状态"（detached）
  // 分离线程：线程结束时自动释放资源，不需要 pthread_join
  // 非分离线程：需要 pthread_join 等待结束并回收资源
  pthread_attr_init(&threadAttr_);
  pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);

  // 【初始化互斥锁】
  // 用于保护 done 标志的线程安全访问
  pthread_mutex_init(&g_ctx.lock, NULL);

  // 【获取 MainActivity 的类】
  // GetObjectClass 获取对象所属的类
  // 返回的是局部引用，需要转换为全局引用
  jclass clz = env->GetObjectClass(instance);
  
  // 【创建全局引用】
  // NewGlobalRef 创建对象的全局引用
  // 将 jclass 从局部引用转为全局引用，供 Native 线程使用
  g_ctx.mainActivityClz = static_cast<jclass>(env->NewGlobalRef(clz));
  
  // 【创建 MainActivity 实例的全局引用】
  // 保存 MainActivity 实例，供 UpdateTicks 线程回调
  g_ctx.mainActivityObj = env->NewGlobalRef(instance);

  // 【创建线程】
  // pthread_create 创建新线程
  // 参数：
  //   1. &threadInfo_ - 接收线程 ID
  //   2. &threadAttr_ - 线程属性
  //   3. UpdateTicks - 线程入口函数
  //   4. &g_ctx - 传递给线程的参数
  int result = pthread_create(&threadInfo_, &threadAttr_, UpdateTicks, &g_ctx);
  
  // 【断言检查】
  // 如果线程创建失败，终止程序（调试模式）
  // 生产代码应该使用错误处理而不是 assert
  assert(result == 0);

  // 【销毁线程属性对象】
  // 属性对象不再需要，释放内存
  pthread_attr_destroy(&threadAttr_);

  // 【消除未使用变量警告】
  (void)result;
}

// ============================================================
// 【StopTicks - 停止计时器线程】
// ============================================================

/**
 * 【StopTicks - Java 调用的 Native 方法，停止后台线程】
 * 
 * 【调用时机】
 * 从 MainActivity.onPause() 调用，当 Activity 进入后台时停止计时
 * 
 * 【功能】
 * 1. 设置 done 标志，通知线程退出
 * 2. 等待线程实际结束（忙等待）
 * 3. 释放全局引用
 * 4. 销毁互斥锁
 * 
 * 【为什么需要等待线程结束？】
 * 如果立即返回，而线程还在运行，可能导致：
 * - 访问已释放的内存
 * - 调用已销毁的对象方法
 * - 应用崩溃
 * 
 * 【优雅的线程停止模式】
 * 1. 设置标志位（done = 1）
 * 2. 线程检测到标志，自行退出
 * 3. 等待线程结束
 * 4. 清理资源
 * 
 * 不要用 pthread_kill！强制终止线程会导致资源泄漏
 * 
 * @param env - JNI 环境指针
 * @param thiz - MainActivity 实例（未使用）
 */
void StopTicks(JNIEnv* env, jobject) {
  // 【发送停止信号】
  // 使用互斥锁保护，确保线程安全
  pthread_mutex_lock(&g_ctx.lock);
  g_ctx.done = 1;  // 设置停止标志
  pthread_mutex_unlock(&g_ctx.lock);

  // 【等待线程结束】
  // 使用忙等待（busy wait）轮询 done 标志
  // 线程结束时会将 done 重置为 0
  // 
  // 【注意】这不是最高效的方式，更好的方式是：
  // - 使用条件变量（pthread_cond_t）
  // - 使用 pthread_join（如果不是 detached 线程）
  struct timespec sleepTime;
  memset(&sleepTime, 0, sizeof(sleepTime));
  sleepTime.tv_nsec = 100000000;  // 100 毫秒
  while (g_ctx.done) {
    nanosleep(&sleepTime, NULL);  // 短暂睡眠，避免 CPU 空转
  }

  // 【释放全局引用】
  // 必须释放 StartTicks 中创建的全局引用
  // 否则会造成内存泄漏（对象无法被 GC）
  env->DeleteGlobalRef(g_ctx.mainActivityClz);
  env->DeleteGlobalRef(g_ctx.mainActivityObj);
  
  // 【清空指针】
  // 良好的编程习惯，避免悬挂指针
  g_ctx.mainActivityObj = NULL;
  g_ctx.mainActivityClz = NULL;

  // 【销毁互斥锁】
  // 释放互斥锁占用的资源
  pthread_mutex_destroy(&g_ctx.lock);
}

// ============================================================
// 【JNI_OnLoad - 库加载入口】
// ============================================================

/**
 * 【JNI_OnLoad - Native 库初始化入口】
 * 
 * 【执行时机】
 * 当 Java 代码执行 System.loadLibrary() 时，JVM 自动调用此函数
 * 
 * 【主要任务】
 * 1. 缓存 JavaVM 指针（供后续 AttachCurrentThread 使用）
 * 2. 注册 Native 方法（RegisterNatives）
 * 3. 查找并缓存 JniHandler 类
 * 4. 创建 JniHandler 实例并保存全局引用
 * 5. 调用 queryRuntimeInfo 演示 JNI 回调
 * 
 * 【资源管理说明】
 * 本函数分配的全局引用（JniHandler 类和实例）在应用生命周期内不释放
 * 原因：
 * - JNI_OnUnload 在 Android 上几乎不会被调用
 * - 应用结束时，系统会自动回收所有全局引用
 * - 这些资源需要跨多个 Activity 生命周期使用
 * 
 * 【关于 JNI_OnUnload】
 * 理论上 JNI_OnUnload 在库卸载时调用，但 Android 上：
 * - 应用退出时直接杀死进程，不会优雅卸载库
 * - 所以 JNI_OnUnload 实际上不会执行
 * 
 * @param vm - JavaVM 指针
 * @param reserved - 保留参数
 * @return jint - JNI 版本号
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  JNIEnv* env;
  
  // 【清零全局上下文】
  // 确保所有指针初始为 NULL，避免未初始化访问
  memset(&g_ctx, 0, sizeof(g_ctx));

  // 【保存 JavaVM 指针】
  // JavaVM 是线程无关的，可以在任何线程使用
  // 主要用于 AttachCurrentThread/DetachCurrentThread
  g_ctx.javaVM = vm;
  
  // 【获取 JNI 环境】
  // 检查 JNI 版本兼容性
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;  // JNI 版本不支持
  }

  // 【查找 MainActivity 类】
  // 用于注册 Native 方法
  jclass c = env->FindClass("com/example/hellojnicallback/MainActivity");
  if (c == nullptr) return JNI_ERR;

  // 【定义 Native 方法映射表】
  // 将 Java native 方法与 C++ 函数绑定
  static const JNINativeMethod methods[] = {
      // stringFromJNI: 返回字符串，演示基础 JNI
      {"stringFromJNI", "()Ljava/lang/String;",
       reinterpret_cast<void*>(StringFromJni)},
      // startTicks: 启动计时器线程
      {"startTicks", "()V", reinterpret_cast<void*>(StartTicks)},
      // StopTicks: 停止计时器线程（注意 Java 端命名大小写不一致）
      {"StopTicks", "()V", reinterpret_cast<void*>(StopTicks)},
  };
  
  // 【注册 Native 方法】
  int rc = env->RegisterNatives(c, methods, arraysize(methods));
  if (rc != JNI_OK) return rc;

  // 【查找 JniHandler 类】
  // 这是一个辅助类，用于处理 Native 层的回调
  jclass clz = env->FindClass("com/example/hellojnicallback/JniHandler");
  
  // 【创建 JniHandler 类的全局引用】
  // 需要长期保存，因为要在 Native 线程中使用
  g_ctx.jniHandlerClz = static_cast<jclass>(env->NewGlobalRef(clz));

  // 【获取构造函数】
  // "<init>" 是 JNI 中构造函数的特殊名称
  // "()V" 表示无参数，返回 void（构造函数总是返回 void）
  jmethodID jniHandlerCtor =
      env->GetMethodID(g_ctx.jniHandlerClz, "<init>", "()V");
  
  // 【创建 JniHandler 实例】
  // NewObject 调用构造函数创建对象
  jobject handler = env->NewObject(g_ctx.jniHandlerClz, jniHandlerCtor);
  
  // 【创建实例的全局引用】
  g_ctx.jniHandlerObj = env->NewGlobalRef(handler);
  
  // 【演示 JNI 回调 Java】
  // 查询并打印 Android 版本和内存信息
  queryRuntimeInfo(env, g_ctx.jniHandlerObj);

  // 【初始化状态】
  g_ctx.done = 0;  // 线程未启动
  g_ctx.mainActivityObj = NULL;  // 等待 StartTicks 设置
  
  return JNI_VERSION_1_6;
}
