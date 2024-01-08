#include <iostream>
#include <cstring>
#include <jni.h>

// extern是一个关键字，用于声明变量或者函数，声明过的可以在其他文件中使用
extern "C"
{

    JNIEXPORT void JNICALL Java_com_ext_HostCache_put(JNIEnv *env, jobject thiz, jstring cacheKey, jbyteArray data);

    JNIEXPORT jbyteArray JNICALL Java_com_ext_HostCache_get(JNIEnv *env, jobject thiz, jstring cacheKey, jlong length);

    JNIEXPORT void JNICALL Java_com_ext_HostCache_delKey(JNIEnv *env, jobject thiz, jstring cacheKey);

    JNIEXPORT void JNICALL Java_com_ext_HostCache_clear(JNIEnv *env, jobject thiz);
}

// 用来设置缓存空间的大小的上限
size_t cacheSizeMax;
// 已经使用的内存大小
std::atomic<long> usedCacheSize;

JNIEXPORT void JNICALL Java_com_ext_HostCache_containerMaxUseMemSize(JNIEnv *env, jobject thiz, jlong maxUsableBytes) {
    // 设置缓存的最大空间
    cacheSizeMax = (size_t)maxUsableBytes;
}

std::unordered_map<std::string, void*> memoryMap;

// 把数据放入内存中
JNIEXPORT void JNICALL Java_com_ext_HostCache_put(JNIEnv* env, jobject thiz, jstring cacheKey, jbyteArray data) {
    // 要存入的对象的大小
    jsize dataSize = env->GetArrayLength(data);
    // 检查当前的内存是否已经达到上限
    if(usedCacheSize + dataSize >= cacheSizeMax){
        // todo 上报java程序内存不足
        return;
    }
    
    const char* nativeName = env->GetStringUTFChars(cacheKey, 0);
    jbyte* nativeData = env->GetByteArrayElements(data, NULL);

    // 计算已经使用的内存大小,这个计算需要保证原子性，否则会出现值覆盖问题，导致计算错误
    // usedCacheSize原子类包装的对象
    usedCacheSize += dataSize;

    void* directMemory = std::malloc(dataSize); // 在C++中分配内存
    if (directMemory != nullptr) {
        memoryMap[std::string(nativeName)] = directMemory;
        std::memcpy(directMemory, nativeData, dataSize);  // 将数据拷贝到内存
    }

    env->ReleaseStringUTFChars(cacheKey, nativeName);
    env->ReleaseByteArrayElements(data, nativeData, JNI_ABORT);
}

// 从内存读取数据
JNIEXPORT jbyteArray JNICALL Java_com_ext_HostCache_get(JNIEnv *env, jobject thiz, jstring cacheKey, jlong length)
{
    const char *nativeName = env->GetStringUTFChars(cacheKey, 0);
    // 查找缓存数据
    auto it = memoryMap.find(std::string(nativeName));
    if (it != memoryMap.end())
    {
        void *directMemory = it->second;
        jbyteArray result = env->NewByteArray(length);
        // 将内存内容复制到Java的byte[]中
        env->SetByteArrayRegion(result, 0, length, reinterpret_cast<jbyte *>(directMemory));
        // 释放cacheName占用的内存空间
        env->ReleaseStringUTFChars(cacheKey, nativeName);
        return result;
    }
    // 读不到数据也需要释放
    env->ReleaseStringUTFChars(cacheKey, nativeName);
    return nullptr;
}

// 从缓存中删除某个key对应的value
JNIEXPORT void JNICALL Java_com_ext_HostCache_delKey(JNIEnv *env, jobject thiz, jstring cacheKey)
{
    const char *nativeName = env->GetStringUTFChars(cacheKey, 0);
    auto it = memoryMap.find(std::string(nativeName));
    if (it != memoryMap.end())
    {
        // 释放内存
        std::free(it->second);
        memoryMap.erase(it);
    }
    env->ReleaseStringUTFChars(cacheKey, nativeName);
}

// 定义一个全局的互斥锁
std::mutex mtx;
// 释放整个内存空间
JNIEXPORT void JNICALL Java_com_ext_HostCache_clear(JNIEnv *env, jobject thiz)
{
    std::lock_guard<std::mutex> lock(mtx); // 锁住互斥锁，确保原子操作
    for (auto it = memoryMap.begin(); it != memoryMap.end(); ++it)
    {
        std::free(it->second); // 释放内存
    }
    memoryMap.clear(); // 清空 map
}