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
// 【包声明】
// ============================================================
package com.example.hellojnicallback;

// ============================================================
// 【导入语句】
// ============================================================

// 【Build 类】
// 提供 Android 系统信息，如版本号、SDK 版本等
import android.os.Build;

// 【Keep 注解】
// 告诉 ProGuard/R8 不要混淆/删除这个类或方法
// 因为 JNI 通过字符串名称调用，混淆后会导致找不到方法
import androidx.annotation.Keep;

// 【日志工具】
import android.util.Log;

// ============================================================
// 【JniHandler - JNI 回调处理器】
// ============================================================

/**
 * 【JniHandler - 供 Native 代码回调的辅助类】
 * 
 * 【设计目的】
 * 这个类专门用于接收 Native 层的回调，演示 JNI 可以调用：
 * 1. private 非静态方法（updateStatus）
 * 2. public 非静态方法（getRuntimeMemorySize）
 * 3. public 静态方法（getBuildVersion）
 * 
 * 【JNI 调用特点】
 * - JNI 不受 Java 访问修饰符限制，可以调用 private 方法
 * - 但反射调用 private 方法需要 setAccessible(true)
 * - JNI 直接调用，无需额外设置
 * 
 * 【@Keep 注解说明】
 * 为什么需要 @Keep？
 * - JNI 通过方法名（字符串）查找 Java 方法
 * - ProGuard/R8 混淆会重命名方法
 * - 混淆后 JNI 找不到方法，导致崩溃
 * - @Keep 确保类名和方法名保持不变
 */
public class JniHandler {
    
    // ============================================================
    // 【updateStatus - 接收 Native 状态消息】
    // ============================================================
    
    /**
     * 【updateStatus - 接收 Native 层的状态消息】
     * 
     * 【访问级别】private
     * 演示 JNI 可以调用 private 方法
     * 
     * 【调用方】
     * Native 层的 sendJavaMsg() 函数
     * 在 UpdateTicks 线程中被调用
     * 
     * 【功能】
     * 将 Native 层的消息输出到 logcat
     * 如果消息包含 "error"，使用 ERROR 级别，否则使用 INFO 级别
     * 
     * @param msg - Native 层传递的消息字符串
     */
    @Keep  // 防止被 ProGuard 删除或重命名
    private void updateStatus(String msg) {
        // 【根据消息内容选择日志级别】
        // 包含 "error" 的消息使用 Log.e（错误级别）
        // 其他消息使用 Log.i（信息级别）
        if (msg.toLowerCase().contains("error")) {
            Log.e("JniHandler", "Native Err: " + msg);
        } else {
            Log.i("JniHandler", "Native Msg: " + msg);
        }
    }

    // ============================================================
    // 【getBuildVersion - 获取 Android 版本】
    // ============================================================
    
    /**
     * 【getBuildVersion - 获取 Android 系统版本号】
     * 
     * 【访问级别】static public
     * 演示 JNI 调用静态方法
     * 
     * 【调用方】
     * Native 层的 queryRuntimeInfo() 函数
     * 
     * 【JNI 调用方式】
     * env->CallStaticObjectMethod(jniHandlerClz, versionFunc)
     * 注意：静态方法只需要类引用，不需要实例
     * 
     * @return String - Android 版本号，如 "13"、"14"
     */
    @Keep
    static public String getBuildVersion() {
        // Build.VERSION.RELEASE 是系统属性
        // 例如：Android 14 返回 "14"
        return Build.VERSION.RELEASE;
    }

    // ============================================================
    // 【getRuntimeMemorySize - 获取可用内存】
    // ============================================================
    
    /**
     * 【getRuntimeMemorySize - 获取 JVM 空闲内存】
     * 
     * 【访问级别】public
     * 演示 JNI 调用实例方法
     * 
     * 【调用方】
     * Native 层的 queryRuntimeInfo() 函数
     * 
     * 【JNI 调用方式】
     * env->CallLongMethod(instance, memFunc)
     * 注意：实例方法需要对象实例
     * 
     * 【返回值类型】
     * JNI 签名 "()J" 表示返回 long（J 是 long 的缩写）
     * 
     * @return long - JVM 空闲内存字节数
     */
    @Keep
    public long getRuntimeMemorySize() {
        // Runtime.getRuntime() 获取当前运行时对象
        // freeMemory() 返回 JVM 空闲内存（字节）
        return Runtime.getRuntime().freeMemory();
    }
}
