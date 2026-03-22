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
 */

// ============================================================
// 【包声明】
// ============================================================
// 定义类的完整包名：com.example.hellojni
// 这个包名必须与 C++ 代码中 FindClass 使用的路径一致
// C++ 中使用："com/example/hellojni/HelloJni"（用 / 替换 .）
package com.example.hellojni

// ============================================================
// 【导入语句】
// ============================================================
// Android 核心类，用于 Activity 生命周期管理
import android.os.Bundle

// AppCompatActivity 是兼容性 Activity 基类，支持旧版本 Android
import androidx.appcompat.app.AppCompatActivity

// ViewBinding 生成的绑定类，用于替代 findViewById
// 在 build.gradle 中启用：viewBinding true
import com.example.hellojni.databinding.ActivityHelloJniBinding

// ============================================================
// 【主 Activity 类】
// ============================================================
/**
 * 【HelloJni - 主 Activity 类】
 * 
 * 这个类演示了如何在 Android 中调用 Native (C/C++) 代码
 * 是学习 JNI (Java Native Interface) 的入门示例
 * 
 * 【JNI 调用流程】
 * 1. 使用 System.loadLibrary() 加载 native 库
 * 2. 声明 external 方法（即 native 方法）
 * 3. 调用 native 方法，实际执行 C++ 代码
 * 4. C++ 代码通过 JNI 返回结果给 Java/Kotlin
 */
class HelloJni : AppCompatActivity() {

    // ============================================================
    // 【Activity 生命周期】
    // ============================================================
    /**
     * 【onCreate - Activity 创建时调用】
     * 
     * @param savedInstanceState 保存的实例状态，用于恢复 Activity
     * 
     * 【ViewBinding 说明】
     * ViewBinding 是 Android 官方推荐的视图绑定方式
     * 优点：
     * - 编译时类型安全，避免空指针和类型转换错误
     * - 替代 findViewById，代码更简洁
     * - 只绑定布局中声明的视图，性能更好
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        // 调用父类实现，完成 Activity 基础初始化
        super.onCreate(savedInstanceState)
        
        /*
         * 【创建视图绑定实例】
         * ActivityHelloJniBinding 是根据 activity_hello_jni.xml 自动生成的类
         * inflate(layoutInflater) 将 XML 布局解析为视图对象
         */
        val binding = ActivityHelloJniBinding.inflate(layoutInflater)
        
        // 【设置 Activity 的内容视图】
        // binding.root 返回布局的根视图（通常是 ConstraintLayout 或 LinearLayout）
        setContentView(binding.root)
        
        // 【调用 Native 方法并显示结果】
        // stringFromJNI() 是在 C++ 中实现的方法
        // 调用时会执行 hello-jni.cpp 中的 StringFromJni 函数
        binding.helloTextview.text = stringFromJNI()
    }

    // ============================================================
    // 【Native 方法声明】
    // ============================================================
    /**
     * 【声明 Native 方法】
     * 
     * 【external 关键字】
     * - 在 Kotlin 中使用 external 声明 native 方法
     * - 在 Java 中使用 native 关键字
     * - 表示该方法的实现不在 JVM 中，而是在 native 库（.so 文件）中
     * 
     * 【方法对应关系】
     * Java/Kotlin 声明：external fun stringFromJNI(): String?
     * C++ 实现：jstring StringFromJni(JNIEnv* env, jobject thiz)
     * 
     * 【JNI 注册方式】
     * 本示例使用 RegisterNatives 方式（推荐）：
     * - 在 JNI_OnLoad 中显式注册方法映射
     * - 性能好，支持代码混淆
     * 
     * 另一种方式是命名约定（传统方式）：
     * - 函数名必须遵循：Java_包名_类名_方法名
     * - 例如：Java_com_example_hellojni_HelloJni_stringFromJNI
     * - 性能稍差，不支持混淆
     * 
     * @return String? - 返回从 C++ 获取的字符串，可能为 null
     */
    external fun stringFromJNI(): String?

    /**
     * 【未实现的 Native 方法示例】
     * 
     * 这个方法故意没有在 C++ 中实现，用于演示错误处理
     * 如果调用此方法，会抛出 UnsatisfiedLinkError 异常
     * 
     * 【异常说明】
     * java.lang.UnsatisfiedLinkError 表示：
     * - native 库未加载
     * - 方法名/签名不匹配
     * - C++ 实现缺失
     * 
     * 【最佳实践】
     * 生产代码中应该避免声明未实现的方法
     * 或者使用 try-catch 捕获异常并优雅处理
     */
    external fun unimplementedStringFromJNI(): String?

    // ============================================================
    // 【静态初始化块】
    // ============================================================
    /**
     * 【companion object - Kotlin 的静态成员容器】
     * 
     * 在 Kotlin 中，companion object 用于定义类的静态成员
     * 相当于 Java 的 static 关键字
     * 
     * 【init 块】
     * init 块在类加载时执行，且只执行一次
     * 是加载 native 库的最佳时机
     */
    companion object {
        /**
         * 【加载 Native 库】
         * 
         * 【System.loadLibrary() 说明】
         * - 参数 "hello-jni" 是库名（不含 lib 前缀和 .so 后缀）
         * - 实际加载的文件是：libhello-jni.so
         * - 库文件在 APK 的 lib/<abi>/ 目录中
         * - 安装时系统会将对应架构的库解压到 /data/data/<package>/lib/
         * 
         * 【ABI (Application Binary Interface)】
         * Android 支持多种 CPU 架构，每种对应不同的 .so 文件：
         * - armeabi-v7a: 32位 ARM（较旧的设备）
         * - arm64-v8a: 64位 ARM（现代主流设备）
         * - x86: 32位 Intel（模拟器常用）
         * - x86_64: 64位 Intel
         * 
         * 【加载时机】
         * - 通常在 static 块中加载，确保类使用时库已就绪
         * - 也可以在 Application.onCreate() 中加载
         * - 多次调用 loadLibrary 不会重复加载
         * 
         * 【错误处理 - 重要！】
         * loadLibrary() 失败会抛出 UnsatisfiedLinkError（继承自 Error）
         * 
         * 常见失败原因：
         * 1. JNI_OnLoad 返回 JNI_ERR → "JNI_ERR returned from JNI_OnLoad"
         * 2. 找不到 so 文件 → "library \"hello-jni\" not found"
         * 3. 找不到 native 方法 → "No implementation found for..."
         * 4. ABI 不匹配 → 在 x86 模拟器运行 arm64 库
         * 
         * 【捕获方式】
         * UnsatisfiedLinkError 是 Error，不是 Exception！
         * 
         * ❌ 错误：捕获不住
         *     try {
         *         System.loadLibrary("hello-jni")
         *     } catch (e: Exception) {  // 捕获不到！
         *         // 不会执行
         *     }
         * 
         * ✅ 正确：可以捕获
         *     try {
         *         System.loadLibrary("hello-jni")
         *     } catch (e: UnsatisfiedLinkError) {  // 捕获具体错误
         *         Log.e("JNI", "加载失败", e)
         *     }
         * 
         * ✅ 也可以：
         *     } catch (e: Error) {  // 捕获所有 Error
         *     } catch (e: Throwable) {  // 捕获所有（包括 Error 和 Exception）
         *     }
         * 
         * 【注意事项】
         * - 库名区分大小写
         * - 如果加载失败会抛出 UnsatisfiedLinkError
         * - 必须先加载依赖的库（如库A依赖库B，先加载B）
         */
        init {
            System.loadLibrary("hello-jni")
        }
    }
}

