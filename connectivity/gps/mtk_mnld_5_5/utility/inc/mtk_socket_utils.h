// This source code is generated by UdpGeneratorTool, not recommend to modify it directly
#ifndef __MTK_SOCKET_UTILS_H__
#define __MTK_SOCKET_UTILS_H__

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

//#define strings void*

#ifndef UNUSED
#define UNUSED(x) (x)=(x)
#endif

typedef enum {
    SOCK_NS_ABSTRACT = 0,
    SOCK_NS_FILESYSTEM = 1,
} mtk_socket_namespace;

typedef struct {
    int fd;
    bool is_local;
    pthread_mutex_t mutex;

    // Network
    char* host;
    int port;

    // Local
    char* path;
    mtk_socket_namespace namesapce;
} mtk_socket_fd;

void _mtk_socket_log(int type, const char *fmt, ...);

#define SOCK_LOGD(...) _mtk_socket_log(0, __VA_ARGS__);
#define SOCK_LOGE(...) _mtk_socket_log(1, __VA_ARGS__);

//-1 means failure
int mtk_socket_server_bind_network(int port);
//-1 means failure
int mtk_socket_server_bind_network_ipv6(int port);
//-1 means failure
int mtk_socket_server_bind_local(const char* path, mtk_socket_namespace sock_namespace);

void mtk_socket_client_init_network(mtk_socket_fd* sock_fd, const char* host, int port);
void mtk_socket_client_init_local(mtk_socket_fd* sock_fd, const char* path, mtk_socket_namespace sock_namespace);

void mtk_socket_client_cleanup(mtk_socket_fd* sock_fd);

bool mtk_socket_client_connect(mtk_socket_fd* sock_fd);
void mtk_socket_client_close(mtk_socket_fd* sock_fd);

//-1 means failure
int mtk_socket_read(int fd, char* buff, int len);

//-1 means failure
int mtk_socket_write(int fd, void* buff, int len);

// <= 0 means no data received
int mtk_socket_poll(int fd, int timeout);


void mtk_socket_buff_dump(const char* buff, int len);

bool mtk_socket_string_is_equal(char* data1, char* data2);
bool mtk_socket_string_array_is_equal(void* data1, int data1_size, void* data2, int data2_size, int string_size);
void mtk_socket_string_array_dump(void* input, int size1, int size2);

bool mtk_socket_bool_array_is_equal(bool data1[], int data1_size, bool data2[], int data2_size);
void mtk_socket_bool_array_dump(bool input[], int size);

bool mtk_socket_char_array_is_equal(char data1[], int data1_size, char data2[], int data2_size);
void mtk_socket_char_array_dump(char input[], int size);

bool mtk_socket_short_array_is_equal(short data1[], int data1_size, short data2[], int data2_size);
void mtk_socket_short_array_dump(short input[], int size);

bool mtk_socket_int_array_is_equal(int data1[], int data1_size, int data2[], int data2_size);
void mtk_socket_int_array_dump(int input[], int size);

bool mtk_socket_int64_t_array_is_equal(int64_t data1[], int data1_size, int64_t data2[], int data2_size);
void mtk_socket_int64_t_array_dump(int64_t input[], int size);

bool mtk_socket_float_array_is_equal(float data1[], int data1_size, float data2[], int data2_size);
void mtk_socket_float_array_dump(float input[], int size);

bool mtk_socket_double_array_is_equal(double data1[], int data1_size, double data2[], int data2_size);
void mtk_socket_double_array_dump(double input[], int size);

bool mtk_socket_expected_bool(char* buff, int* offset, bool expected_value, const char* func, int line);
bool mtk_socket_expected_char(char* buff, int* offset, char expected_value, const char* func, int line);
bool mtk_socket_expected_short(char* buff, int* offset, short expected_value, const char* func, int line);
bool mtk_socket_expected_int(char* buff, int* offset, int expected_value, const char* func, int line);
bool mtk_socket_expected_int64_t(char* buff, int* offset, int64_t expected_value, const char* func, int line);
bool mtk_socket_expected_float(char* buff, int* offset, float expected_value, const char* func, int line);
bool mtk_socket_expected_double(char* buff, int* offset, double expected_value, const char* func, int line);
bool mtk_socket_expected_string(char* buff, int* offset, const char* expected_value, int max_size, const char* func, int line);

#define ASSERT_EQUAL_INT(d1, d2) \
{\
    int _d1 = d1;\
    int _d2 = d2;\
    if(_d1 != _d2) {\
        SOCK_LOGE("%s():%d ASSERT_EQUAL_INT() failed, d1=[%d], d2=[%d]",\
        __func__, __LINE__, _d1, _d2);\
        return false;\
    }\
}

#define EXPECTED_BOOL(buff, offset, expected_value) \
{\
    if(!mtk_socket_expected_bool(buff, offset, expected_value, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_CHAR(buff, offset, expected_value) \
{\
    if(!mtk_socket_expected_char(buff, offset, expected_value, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_SHORT(buff, offset, expected_value) \
{\
    if(!mtk_socket_expected_short(buff, offset, expected_value, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_INT(buff, offset, expected_value) \
{\
    if(!mtk_socket_expected_int(buff, offset, expected_value, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_INT64_T(buff, offset, expected_value) \
{\
    if(!mtk_socket_expected_int64_t(buff, offset, expected_value, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_FLOAT(buff, offset, expected_value) \
{\
    if(!mtk_socket_expected_float(buff, offset, expected_value, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_DOUBLE(buff, offset, expected_value) \
{\
    if(!mtk_socket_expected_double(buff, offset, expected_value, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_STRING(buff, offset, expected_value, max_size) \
{\
    if(!mtk_socket_expected_string(buff, offset, expected_value, max_size, __func__, __LINE__)) {\
        return false;\
    }\
}

#define EXPECTED_STRUCT(buff, offset, expected_value, struct_name) \
{\
    struct_name _tmp;\
    struct_name##_decode(buff, offset, &_tmp);\
    if(!struct_name##_is_equal(&_tmp, expected_value)) {\
        SOCK_LOGE("%s():%d EXPECTED_STRUCT() %s_is_equal() fail", __func__, __LINE__, #struct_name);\
        SOCK_LOGE(" ============= read ===============");\
        struct_name##_dump(&_tmp);\
        SOCK_LOGE(" ============= expected ===============");\
        struct_name##_dump(expected_value);\
        return false;\
    }\
}

#define EXPECTED_ARRAY(buff, offset, expected_num, expected_value, type, max_size) \
{\
    type _tmp[max_size] = {0};\
    int _tmp_num = mtk_socket_get_##type##_array(buff, offset, _tmp, max_size);\
    if(!mtk_socket_##type##_array_is_equal(expected_value, expected_num, _tmp, _tmp_num)) {\
        SOCK_LOGE("%s():%d expected_%s_array() fail", __func__, __LINE__, #type);\
        SOCK_LOGE(" ============= read ===============");\
        mtk_socket_##type##_array_dump(_tmp, _tmp_num);\
        SOCK_LOGE(" ============= expected ===============");\
        mtk_socket_##type##_array_dump(expected_value, expected_num);\
        return false;\
    }\
}

#define EXPECTED_STRING_ARRAY(buff, offset, expected_num, expected_value, max_size1, max_size2) \
{\
    char _tmp[max_size1][max_size2];\
    int _tmp_num = mtk_socket_get_string_array(buff, offset, _tmp, max_size1, max_size2);\
    if(!mtk_socket_string_array_is_equal(expected_value, expected_num, _tmp, _tmp_num, max_size2)) {\
        SOCK_LOGE("%s():%d expected_string_array() fail", __func__, __LINE__);\
        SOCK_LOGE(" ============= read ===============");\
        mtk_socket_string_array_dump(_tmp, _tmp_num, max_size2);\
        SOCK_LOGE(" ============= expected ===============");\
        mtk_socket_string_array_dump(expected_value, expected_num, max_size2);\
        return false;\
    }\
}

#define EXPECTED_STRUCT_ARRAY(buff, offset, expected_num, expected_value, type, max_size) \
{\
    type _tmp[max_size];\
    int _tmp_num = type##_array_decode(buff, offset, _tmp, max_size);\
    if(!type##_array_is_equal(expected_value, expected_num, _tmp, _tmp_num)) {\
        SOCK_LOGE("%s():%d expected_%s_array() fail", __func__, __LINE__, #type);\
        SOCK_LOGE(" ============= read ===============");\
        type##_array_dump(_tmp, _tmp_num);\
        SOCK_LOGE(" ============= expected ===============");\
        type##_array_dump(expected_value, expected_num);\
        return false;\
    }\
}

#define ASSERT_LESS_EQUAL_THAN(d1, d2) \
{\
    if(d1 > d2) {\
        SOCK_LOGE("%s():%d ASSERT_LESS_EQUAL_THAN() fail, %s=[%d] > [%d]", __func__, __LINE__, #d1, d1, d2);\
        return false;\
    }\
}

#ifdef __cplusplus
}
#endif

#endif
