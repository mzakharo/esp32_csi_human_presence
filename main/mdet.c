#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "mdet.h"

#define HISTORY_SIZE 20
#define NUM_SUBCARRIERS 26
#define EPSILON 1e-10f


typedef struct {
    float temporal_variance;
    float subcarrier_correlation;
} Features;



// Circular buffer operations
void init_circular_buffer(CircularBuffer* buffer, int capacity) {
    buffer->data = (float*)calloc(capacity * NUM_SUBCARRIERS, sizeof(float));
    buffer->size = 0;
    buffer->capacity = capacity;
    buffer->start = 0;
}

void free_circular_buffer(CircularBuffer* buffer) {
    free(buffer->data);
}

void push_circular_buffer(CircularBuffer* buffer, float* values) {
    int end = (buffer->start + buffer->size) % buffer->capacity;
    memcpy(&buffer->data[end * NUM_SUBCARRIERS], values, NUM_SUBCARRIERS * sizeof(float));
    
    if (buffer->size < buffer->capacity) {
        buffer->size++;
    } else {
        buffer->start = (buffer->start + 1) % buffer->capacity;
    }
}

// Statistical functions
float mean(float* data, int size) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum / size;
}

float variance(float* data, int size, float mean_val) {
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        float diff = data[i] - mean_val;
        sum += diff * diff;
    }
    return sum / size;
}

float standard_deviation(float* data, int size, float mean_val) {
    return sqrtf(variance(data, size, mean_val));
}

// CSI Magnitude Detector functions
CSIMagnitudeDetector* create_detector(int window_size, float threshold) {
    CSIMagnitudeDetector* detector = (CSIMagnitudeDetector*)malloc(sizeof(CSIMagnitudeDetector));
    detector->window_size = window_size;
    detector->threshold = threshold;
    detector->num_subcarriers = NUM_SUBCARRIERS;
    detector->epsilon = EPSILON;
    
    init_circular_buffer(&detector->magnitude_buffer, window_size);
    init_circular_buffer(&detector->history_buffer, HISTORY_SIZE);
    
    return detector;
}

void free_detector(CSIMagnitudeDetector* detector) {
    free_circular_buffer(&detector->magnitude_buffer);
    free_circular_buffer(&detector->history_buffer);
    free(detector);
}

void reject_outliers(CSIMagnitudeDetector* detector, float* magnitude_data, float* output) {
    memcpy(output, magnitude_data, NUM_SUBCARRIERS * sizeof(float));
    push_circular_buffer(&detector->history_buffer, magnitude_data);
    
    if (detector->history_buffer.size >= 5) {
        float local_mean[NUM_SUBCARRIERS] = {0};
        float local_std[NUM_SUBCARRIERS] = {0};
        
        // Calculate local mean and std for each subcarrier
        for (int i = 0; i < NUM_SUBCARRIERS; i++) {
            float sum = 0.0f, sum_sq = 0.0f;
            int count = 0;
            
            for (int j = 0; j < detector->history_buffer.size; j++) {
                int idx = (detector->history_buffer.start + j) % detector->history_buffer.capacity;
                float val = detector->history_buffer.data[idx * NUM_SUBCARRIERS + i];
                sum += val;
                sum_sq += val * val;
                count++;
            }
            
            local_mean[i] = sum / count;
            float variance = (sum_sq / count) - (local_mean[i] * local_mean[i]);
            local_std[i] = sqrtf(fmaxf(variance, detector->epsilon));
        }
        
        // Apply outlier rejection
        for (int i = 0; i < NUM_SUBCARRIERS; i++) {
            float threshold = 3 * local_std[i];
            float deviation = fabsf(magnitude_data[i] - local_mean[i]);
            if (deviation > threshold) {
                output[i] = local_mean[i];
            }
        }
    }
}

void preprocess_magnitude(CSIMagnitudeDetector* detector, float* magnitude_data, float* output) {
    // Reject outliers
    float cleaned[NUM_SUBCARRIERS];
    reject_outliers(detector, magnitude_data, cleaned);
    
    // Remove mean
    float mag_mean = mean(cleaned, NUM_SUBCARRIERS);
    for (int i = 0; i < NUM_SUBCARRIERS; i++) {
        output[i] = cleaned[i] - mag_mean;
    }
    
    // Scale to unit variance
    float std_val = standard_deviation(output, NUM_SUBCARRIERS, 0.0f);
    if (std_val > detector->epsilon) {
        for (int i = 0; i < NUM_SUBCARRIERS; i++) {
            output[i] /= std_val;
        }
    }
}

// [Previous code remains the same until extract_features function]

void extract_features(CSIMagnitudeDetector* detector, Features* features) {
    float temporal_var[NUM_SUBCARRIERS] = {0};
    
    // Calculate temporal variance
    for (int i = 0; i < NUM_SUBCARRIERS; i++) {
        float subcarrier_data[WINDOW_SIZE];
        float subcarrier_mean = 0.0f;
        
        // Gather data for this subcarrier
        for (int j = 0; j < detector->magnitude_buffer.size; j++) {
            int idx = (detector->magnitude_buffer.start + j) % detector->magnitude_buffer.capacity;
            subcarrier_data[j] = detector->magnitude_buffer.data[idx * NUM_SUBCARRIERS + i];
            subcarrier_mean += subcarrier_data[j];
        }
        subcarrier_mean /= detector->magnitude_buffer.size;
        
        temporal_var[i] = variance(subcarrier_data, detector->magnitude_buffer.size, subcarrier_mean);
    }
    
    features->temporal_variance = mean(temporal_var, NUM_SUBCARRIERS);
    
    // Calculate correlation between adjacent subcarriers
    float correlation_sum = 0.0f;
    int correlation_count = 0;
    
    for (int i = 0; i < NUM_SUBCARRIERS - 1; i++) {
        // Calculate means for both subcarriers
        float mean1 = 0.0f, mean2 = 0.0f;
        for (int j = 0; j < detector->magnitude_buffer.size; j++) {
            int idx = (detector->magnitude_buffer.start + j) % detector->magnitude_buffer.capacity;
            mean1 += detector->magnitude_buffer.data[idx * NUM_SUBCARRIERS + i];
            mean2 += detector->magnitude_buffer.data[idx * NUM_SUBCARRIERS + i + 1];
        }
        mean1 /= detector->magnitude_buffer.size;
        mean2 /= detector->magnitude_buffer.size;
        
        // Calculate correlation coefficient
        float numerator = 0.0f;
        float denom1 = 0.0f;
        float denom2 = 0.0f;
        
        for (int j = 0; j < detector->magnitude_buffer.size; j++) {
            int idx = (detector->magnitude_buffer.start + j) % detector->magnitude_buffer.capacity;
            float val1 = detector->magnitude_buffer.data[idx * NUM_SUBCARRIERS + i] - mean1;
            float val2 = detector->magnitude_buffer.data[idx * NUM_SUBCARRIERS + i + 1] - mean2;
            
            numerator += val1 * val2;
            denom1 += val1 * val1;
            denom2 += val2 * val2;
        }
        
        // Add small noise to prevent singular correlation matrix
        numerator += detector->epsilon * ((float)rand() / RAND_MAX - 0.5f);
        denom1 = fmaxf(denom1, detector->epsilon);
        denom2 = fmaxf(denom2, detector->epsilon);
        
        float correlation = numerator / sqrtf(denom1 * denom2);
        correlation_sum += correlation;
        correlation_count++;
    }
    
    features->subcarrier_correlation = correlation_sum / correlation_count;
}

// [Rest of the code remains the same]

float compute_detection_score(Features* features) {
    float norm_variance = fminf(features->temporal_variance / 0.1f, 1.0f);
    float norm_correlation = fminf(fabsf(features->subcarrier_correlation), 1.0f);
    float score = 0.4f * norm_variance + 0.6f * norm_correlation;
    return fminf(fmaxf(score, 0.0f), 1.0f);
}

int detect_presence(CSIMagnitudeDetector* detector, float* magnitude_data, float* confidence) {
    float processed[NUM_SUBCARRIERS];
    preprocess_magnitude(detector, magnitude_data, processed);
    push_circular_buffer(&detector->magnitude_buffer, processed);
    
    if (detector->magnitude_buffer.size < detector->window_size) {
        *confidence = 0.0f;
        return 0;
    }
    
    Features features;
    extract_features(detector, &features);
    *confidence = compute_detection_score(&features);
    
    return *confidence > detector->threshold;
}
