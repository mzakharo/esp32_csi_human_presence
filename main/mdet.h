
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
    CircularBuffer magnitude_buffer;
    CircularBuffer history_buffer;
} CSIMagnitudeDetector;


CSIMagnitudeDetector* create_detector(int window_size, float threshold);
int detect_presence(CSIMagnitudeDetector* detector, float* magnitude_data, float* confidence);