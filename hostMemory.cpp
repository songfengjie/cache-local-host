#include <iostream>
#include <cstring>
#include <jni.h>
#include <calculate.h>

// extern是一个关键字，用于声明变量或者函数，声明过的可以在其他文件中使用
extern "C"
{
    JNIEXPORT void JNICALL Java_com_ext_HostCache_containerMaxUseMemSize(JNIEnv *env, jobject thiz, jlong maxUsableBytes);
    // 设置缓存，如果key相同，那么会释放旧值的地址，放入新值的地址
    JNIEXPORT void JNICALL Java_com_ext_HostCache_put(JNIEnv *env, jobject thiz, jstring cacheKey, jbyteArray data);

    JNIEXPORT jbyteArray JNICALL Java_com_ext_HostCache_get(JNIEnv *env, jobject thiz, jstring cacheKey);

    JNIEXPORT void JNICALL Java_com_ext_HostCache_delKey(JNIEnv *env, jobject thiz, jstring cacheKey);

    JNIEXPORT void JNICALL Java_com_ext_HostCache_clear(JNIEnv *env, jobject thiz);
}

// 用于缓存数据的map
std::unordered_map<std::string, char*> memoryMap;
// 用来设置缓存空间的大小的上限
size_t cacheSizeMax = 0;
// 定义一个全局的互斥锁
std::mutex mtx;

JNIEXPORT void JNICALL Java_com_ext_HostCache_containerMaxUseMemSize(JNIEnv *env, jobject thiz, jlong maxUsableBytes)
{
    // 设置缓存的最大空间
    cacheSizeMax = (size_t)maxUsableBytes;
}
// 计算缓存已经使用的空间大小
size_t calcUsedCacheSize()
{
    size_t totalMemorySize = 0;
    for (auto it = memoryMap.begin(); it != memoryMap.end(); ++it)
    {
        char *pointer = it->second;
        size_t memorySize = GET_MEMORY_SIZE(pointer);
        totalMemorySize += memorySize;
    }
    return totalMemorySize;
}

// 验证内存是否足够
// 0:内存不足，1:内存充足
bool calcOOM(long dataSize)
{
    if (cacheSizeMax == 0)
    {
        // todo 上报内存不足信息
        return 0;
    }
    size_t usedSize = calcUsedCacheSize();
    if (usedSize + dataSize < cacheSizeMax)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// 把数据放入内存中
JNIEXPORT void JNICALL Java_com_ext_HostCache_put(JNIEnv *env, jobject thiz, jstring cacheKey, jbyteArray data)
{
    // 要存入的对象的大小
    jsize dataSize = env->GetArrayLength(data);
    bool can = calcOOM(dataSize);
    if (!can)
    {
        return;
    }
    // cacheKey的字符串指针
    const char *nativeName = env->GetStringUTFChars(cacheKey, 0);
    // 缓存数据的指针
    jbyte *nativeData = env->GetByteArrayElements(data, NULL);
    // 在C++中分配内存
    char *directMemory = new char[dataSize];
    if (directMemory != nullptr)
    {
        // 锁住互斥锁，确保原子操作
        std::lock_guard<std::mutex> lock(mtx);
        auto it = memoryMap.find(std::string(nativeName));
        if (it != memoryMap.end())
        {
            char *pointer = it->second;
            // 释放旧内存（堆地址）
            delete[] pointer;
            // 清理map的value，也就是原先的指针本身
            memoryMap.erase(it);
        }
        memoryMap[std::string(nativeName)] = directMemory;
        std::memcpy(directMemory, nativeData, dataSize); // 将数据拷贝到内存
    }

    env->ReleaseStringUTFChars(cacheKey, nativeName);
    env->ReleaseByteArrayElements(data, nativeData, JNI_ABORT);
}

// 从内存读取数据
JNIEXPORT jbyteArray JNICALL Java_com_ext_HostCache_get(JNIEnv *env, jobject thiz, jstring cacheKey)
{
    const char *nativeName = env->GetStringUTFChars(cacheKey, 0);
    // 查找缓存数据
    auto it = memoryMap.find(std::string(nativeName));
    if (it != memoryMap.end())
    {
        // 目标缓存数据的指针
        char *directMemory = it->second;
        // 计算该内存的大小
        size_t memorySize = GET_MEMORY_SIZE(directMemory);
        // 重新分配一块java对象的内存
        jbyteArray result = env->NewByteArray(memorySize);
        // 将内存内容复制到Java的byte[]中
        env->SetByteArrayRegion(result, 0, memorySize, reinterpret_cast<jbyte *>(directMemory));
        // 释放cacheName占用的内存空间
        env->ReleaseStringUTFChars(cacheKey, nativeName);
        return result;
    }
    // 读不到数据也需要释放
    env->ReleaseStringUTFChars(cacheKey, nativeName);
    // 读不到数据，默认返回null指针
    return nullptr;
}

// 从缓存中删除某个key对应的value
JNIEXPORT void JNICALL Java_com_ext_HostCache_delKey(JNIEnv *env, jobject thiz, jstring cacheKey)
{
    const char *nativeName = env->GetStringUTFChars(cacheKey, 0);
    // 查找当前值是否存在
    auto it = memoryMap.find(std::string(nativeName));
    if (it != memoryMap.end())
    {
        // 持有锁
        std::lock_guard<std::mutex> lock(mtx);
        // 双重检查
        it = memoryMap.find(std::string(nativeName));
        if (it != memoryMap.end())
        {
            // 释放内存
            delete[] it->second;
            memoryMap.erase(it);
        }
    }
    env->ReleaseStringUTFChars(cacheKey, nativeName);
}

// 释放整个内存空间
JNIEXPORT void JNICALL Java_com_ext_HostCache_clear(JNIEnv *env, jobject thiz)
{
    // 锁住互斥锁，确保原子操作
    std::lock_guard<std::mutex> lock(mtx); 
    for (auto it = memoryMap.begin(); it != memoryMap.end(); ++it)
    {
        delete[] it->second;
    }
    memoryMap.clear(); // 清空 map
}