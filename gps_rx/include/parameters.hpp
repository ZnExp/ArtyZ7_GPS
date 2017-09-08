// FM center frequency used to detect oscillator offset (Hz)
const uint32_t FM_CHANNEL = static_cast<uint32_t>(80.0e6);

// GPS L1 C/A carrier frequency (Hz)
const uint32_t L1_CARRIER = static_cast<uint32_t>(1575.42e6);

// RTL-SDR sampling frequency (Hz)
const uint32_t FSAMPLE = static_cast<uint32_t>(2.046e6);

// FFT size
const uint32_t FFTSIZE = 8192;

// Saturation percentage limit (lower the gain if this is exceeded)
const float SAT_PERCENTAGE_LIMIT = 10.0;

// Sampling frequency for frequency offset estimation (Hz)
const float FS_FM = 240.0e3;

// Number of seconds to allow oscillator to stabilize
//const int WAIT_SEC = 120;        
const int WAIT_SEC = 20;

// Number of FFT frames to use for frequenyc offset estimation
const int FERR_FRAMES = 800;

// Number of frequency bins in C/A acquisition search (MUST be an odd number)
const uint32_t NUM_BINS = 67;
