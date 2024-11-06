import numpy as np
from collections import deque

def hampel(x, k=7, t0=2.5):
    """
    Apply the Hampel filter to a 1D numpy array.
    
    Parameters:
    x (numpy.ndarray): 1D input array
    k (int): window size (default is 7)
    t0 (float): threshold (default is 2.5)
    
    Returns:
    numpy.ndarray: 1D array with outliers replaced
    """
    # Compute the median absolute deviation (MAD)
    mad = np.median(np.abs(x - np.median(x)))
    
    # Compute the lower and upper threshold
    lower = x - t0 * k * mad
    upper = x + t0 * k * mad
    
    # Apply the filter
    y = x.copy()
    y[x < lower] = np.median(x)
    y[x > upper] = np.median(x)
    
    return y



class CSIMagnitudeDetector:
    def __init__(self, window_size=50, threshold=0.60):
        """
        Initialize the CSI magnitude-based presence detector
        """
        self.window_size = window_size
        self.threshold = threshold
        self.num_subcarriers = 26
        self.epsilon = 1e-10  # Small constant for numerical stability

        self.var = self.cor = 0
        
        # Buffers
        self.magnitude_buffer = deque(maxlen=window_size)
        self.history_buffer = deque(maxlen=20)
        
    
    def reject_outliers(self, magnitude_data):
        """
        Reject outliers in time domain using multiple methods
        """        
        self.history_buffer.append(magnitude_data)
        
        # Method 1: Adaptive threshold based on local statistics
        if len(self.history_buffer) >= 5:
            recent_history = np.array(self.history_buffer)
            local_mean = np.mean(recent_history, axis=0)
            local_std = np.std(recent_history, axis=0)
            
            # Ensure standard deviation is not too small
            local_std = np.maximum(local_std, self.epsilon)
            
            threshold = 3 * local_std
            deviation = np.abs(magnitude_data - local_mean)
            magnitude_data = np.where(deviation > threshold, local_mean, magnitude_data)
        
        return magnitude_data
    
    def preprocess_magnitude(self, magnitude_data):
        """
        Preprocess CSI magnitude data with outlier rejection
        """
        # Reject outliers
        magnitude_cleaned = self.reject_outliers(magnitude_data)

        #magnitude_cleaned = hampel(magnitude_cleaned)
        
        # Remove mean
        den = np.mean(magnitude_cleaned)
        magnitude_filtered = magnitude_cleaned / den if den > self.epsilon else magnitude_cleaned
        
       
            
        return magnitude_filtered
    
    def extract_features(self, magnitude_window):
        """
        Extract features from preprocessed magnitude data
        """
        features = {}
        
        # Temporal variance
        temporal_var = np.var(magnitude_window, axis=0)
        features['temporal_variance'] = np.mean(temporal_var)        
        
        # Correlation between adjacent subcarriers
        corr_matrix = np.corrcoef(magnitude_window.T)
        features['subcarrier_correlation'] = np.mean(np.diag(corr_matrix, k=1))
        
        return features
    
    def detect_presence(self, magnitude_data):
        """
        Detect human presence from new CSI magnitude measurements
        """
                
        # Preprocess
        magnitude_processed = self.preprocess_magnitude(magnitude_data)
        self.magnitude_buffer.append(magnitude_processed)
        
        if len(self.magnitude_buffer) < self.window_size:
            return False, 0.0
        
        magnitude_window = np.array(self.magnitude_buffer)
        
        # Extract features and compute score
        features = self.extract_features(magnitude_window)
        score, self.var, self.cor = self._compute_detection_score(features)      
        
        return score > self.threshold, score
    
    def _compute_detection_score(self, features):
        """
        Compute presence detection score from features
        """
        # Normalize features with numerical stability
        norm_variance = np.clip(features['temporal_variance']/0.01, 0, 1)
        norm_correlation = np.clip(np.abs(features['subcarrier_correlation']), 0, 1)
        score = (0.4 * norm_variance + 
                0.6 * norm_correlation)
        
        return np.clip(score, 0, 1), norm_variance, norm_correlation
    

# Example usage
def real_time_detection():
    detector = CSIMagnitudeDetector()
    
    while True:
        # Simulate receiving CSI magnitude data (replace with actual data acquisition)
        magnitude_data = np.random.random(26)
        
        is_present, confidence = detector.detect_presence(magnitude_data)
        
        if is_present:
            print(f"Human presence detected! (Confidence: {confidence:.2f})")
        else:
            print(f"No presence detected. (Confidence: {confidence:.2f})")

if __name__ == "__main__":
    real_time_detection()