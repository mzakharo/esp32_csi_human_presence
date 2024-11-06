
#pragma once

#define WINDOW_SIZE 50
typedef struct {
    float* data;
    int size;
    int capacity;
    int start;
} CircularBuffer;

typedef struct {
    int window_size;
    float threshold;
    int num_subcarriers;
    float epsilon;
    float var;
    float cor;
    CircularBuffer magnitude_buffer;
    CircularBuffer history_buffer;
} CSIMagnitudeDetector;

#ifdef __cplusplus
extern "C" {
#endif
    CSIMagnitudeDetector* create_detector(int window_size, float threshold);
    int detect_presence(CSIMagnitudeDetector* detector, float* magnitude_data, float* confidence);

#ifdef __cplusplus
}
#endif
