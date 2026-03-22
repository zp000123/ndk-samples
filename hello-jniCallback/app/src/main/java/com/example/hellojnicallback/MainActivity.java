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
package com.example.hellojnicallback;

// ============================================================
// 【导入语句】
// ============================================================

// 【Keep 注解】
// 防止被 ProGuard/R8 混淆，因为 JNI 通过名称调用这些方法
import androidx.annotation.Keep;

// 【AppCompatActivity】
// 兼容性 Activity 基类
import androidx.appcompat.app.AppCompatActivity;

// 【Bundle】
// 用于保存和恢复 Activity 状态
import android.os.Bundle;

// 【TextView】
// 文本显示控件
import android.widget.TextView;

// ============================================================
// 【MainActivity - 主界面】
// ============================================================

/**
 * 【MainActivity - JNI 回调演示主界面】
 * 
 * 【功能说明】
 * 这个 Activity 演示了 Native 线程如何回调 Java 方法更新 UI
 * 
 * 【工作流程】
 * 1. onResume() → 启动 Native 线程（startTicks）
 * 2. Native 线程每秒调用 updateTimer() 更新计时器
 * 3. onPause() → 停止 Native 线程（StopTicks）
 * 
 * 【线程模型】
 * - UI 线程：Android 主线程，处理用户交互和 UI 更新
 * - Native 线程：pthread 创建的线程，在 C++ 层运行
 * - 线程通信：通过 JNI 回调实现
 * 
 * 【重要说明】
 * Native 线程不能直接操作 UI，必须通过：
 * 1. runOnUiThread()（本示例使用）
 * 2. Handler + Looper
 * 3. View.post()
 */
public class MainActivity extends AppCompatActivity {

    // ============================================================
    // 【成员变量】
    // ============================================================
    
    // 【计时器变量】
    // 存储当前计时的小时、分钟、秒
    // 由 Native 线程通过 updateTimer() 回调递增
    int hour = 0;
    int minute = 0;
    int second = 0;
    
    // 【UI 引用】
    // 显示计时器的 TextView
    TextView tickView;

    // ============================================================
    // 【Activity 生命周期】
    // ============================================================
    
    /**
     * 【onCreate - Activity 创建】
     * 
     * 【功能】
     * 初始化界面布局，获取 View 引用
     * 
     * 【注意】
     * 不在这里启动 Native 线程，因为：
     * - onCreate 在配置变化（如旋转屏幕）时会重新调用
     * - 会导致线程重复创建
     * - 应该在 onResume 中启动，onPause 中停止
     * 
     * @param savedInstanceState - 保存的状态数据
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // 设置布局文件
        setContentView(R.layout.activity_main);
        // 获取显示计时的 TextView
        tickView = (TextView) findViewById(R.id.tickView);
    }

    /**
     * 【onResume - Activity 进入前台】
     * 
     * 【功能】
     * 1. 重置计时器
     * 2. 调用 stringFromJNI() 获取 ABI 信息并显示
     * 3. 启动 Native 计时线程
     * 
     * 【调用时机】
     * - Activity 首次显示
     * - 从其他 Activity 返回
     * - 从后台切换回前台
     * 
     * 【为什么在这里启动线程？】
     * onResume 是 Activity 可见且可交互的标志
     * 此时启动后台任务最合适
     */
    @Override
    public void onResume() {
        super.onResume();
        // 【重置计时器】
        hour = minute = second = 0;
        
        // 【获取并显示 ABI 信息】
        // stringFromJNI() 是 Native 方法，返回编译目标架构
        ((TextView)findViewById(R.id.hellojniMsg)).setText(stringFromJNI());
        
        // 【启动 Native 计时线程】
        // 这会创建一个 pthread 线程，每秒回调 updateTimer()
        startTicks();
    }

    /**
     * 【onPause - Activity 进入后台】
     * 
     * 【功能】
     * 停止 Native 计时线程
     * 
     * 【调用时机】
     * - 跳转到其他 Activity
     * - 按 Home 键返回桌面
     * - 屏幕关闭
     * 
     * 【为什么必须停止线程？】
     * 1. 节省电量：后台不需要计时
     * 2. 避免内存泄漏：Activity 可能很快被销毁
     * 3. 避免崩溃：线程回调时 Activity 可能已销毁
     * 
     * 【注意大小写】
     * Java 端是 StopTicks（大写 S）
     * 但 Native 端方法名是 startTicks（小写 s）
     * 这是历史遗留问题，实际调用通过签名匹配，不受方法名大小写影响
     */
    @Override
    public void onPause () {
        super.onPause();
        StopTicks();
    }

    // ============================================================
    // 【Native 回调方法】
    // ============================================================
    
    /**
     * 【updateTimer - Native 线程回调更新计时器】
     * 
     * 【调用方】
     * Native 层的 UpdateTicks 线程（每秒调用一次）
     * 
     * 【访问级别】private
     * 演示 JNI 可以调用 private 方法
     * 
     * 【线程切换】
     * Native 线程不能直接操作 UI，必须使用 runOnUiThread()
     * 将 UI 更新操作提交到主线程执行
     * 
     * 【@Keep 注解】
     * 防止 ProGuard 删除或重命名此方法
     * 否则 Native 找不到方法，应用崩溃
     */
    @Keep
    private void updateTimer() {
        // 【递增秒数】
        ++second;
        
        // 【进位处理】
        // 60 秒进 1 分钟
        if(second >= 60) {
            ++minute;
            second -= 60;
            // 60 分钟进 1 小时
            if(minute >= 60) {
                ++hour;
                minute -= 60;
            }
        }
        
        // 【切换到 UI 线程更新界面】
        // runOnUiThread 将 Runnable 提交到主线程的消息队列
        // 这是线程安全的 UI 更新方式
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                // 【格式化时间字符串】
                // 格式：HH:MM:SS
                String ticks = "" + MainActivity.this.hour + ":" +
                        MainActivity.this.minute + ":" +
                        MainActivity.this.second;
                
                // 【更新 TextView】
                MainActivity.this.tickView.setText(ticks);
            }
        });
    }

    // ============================================================
    // 【Native 库加载和方法声明】
    // ============================================================
    
    // 【静态初始化块】
    // 类加载时执行，加载 Native 库
    static {
        System.loadLibrary("hello-jnicallback");
    }
    
    // 【Native 方法声明】
    
    /**
     * 【stringFromJNI - 获取 ABI 信息】
     * 
     * 返回编译目标架构信息，如 "arm64-v8a"、"x86" 等
     * 用于验证 so 库是否正确加载
     * 
     * @return String - ABI 信息字符串
     */
    public native String stringFromJNI();
    
    /**
     * 【startTicks - 启动计时器】
     * 
     * 创建 Native 线程，开始每秒回调 updateTimer()
     * 
     * 【注意】
     * 必须在 onResume 中调用，且配对调用 StopTicks
     */
    public native void startTicks();
    
    /**
     * 【StopTicks - 停止计时器】
     * 
     * 停止 Native 线程，释放资源
     * 
     * 【注意】
     * 方法名大小写与 startTicks 不一致（历史遗留）
     * 但 JNI 注册时通过签名匹配，不影响功能
     */
    public native void StopTicks();
}
