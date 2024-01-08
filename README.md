# cache-local-host
c++开辟本地主机内存空间，供java程序作为堆外缓存使用

目前支持：
1、设置缓存空间的上限（缓存空间不得超出上限）:Java_com_ext_HostCache_containerMaxUseMemSize
2、支持设置健值对缓存：Java_com_ext_HostCache_put
3、支持读取缓存：Java_com_ext_HostCache_get
4、支持删除某个缓存：Java_com_ext_HostCache_delKey
5、支持清理整个缓存空间：Java_com_ext_HostCache_clear

后续计划：
1、引入缓存过期策略；
2、缓存结果上报java程序；
3、缓存监控，日志信息记录；