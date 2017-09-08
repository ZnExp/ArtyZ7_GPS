#include <cmath>        // for trig functions
#include <cstdlib>      // for exit, NULL
#include <iostream>     // for cout

#include "rtl-sdr.h"    // for RTL-SDR interface routines
#include "fftw3.h"      // for FFT operations

#include "parameters.hpp"       // operational parameters

#define Real(x) x[0]            // append a [0] index to fftw_complex to indicate the real part
#define Imag(x) x[1]            // append a [1] index to fftw_complex to indicate the imaginary part
#define Square(x) ((x) * (x))   // x^2

using namespace std;

int main(int argc, char *argv[]) {

  cout << endl;
  cout << endl;
  cout << "**************************************************************" << endl;
  cout << "*                                                            *" << endl;
  cout << "* m(_ _)m Welcome to the Arty Z7 board with RTL-SDR! m(_ _)m *" << endl;
  cout << "*                                                            *" << endl;
  cout << "**************************************************************" << endl;
  cout << endl;
  cout << endl;

  cout << endl;
  cout << "Checking for connected RTL-SDR devices..." << endl;
  uint32_t num_rtlsdrs = rtlsdr_get_device_count();	// how many RTL-SDR dongles are connected?
	
  cout << endl;
  if (num_rtlsdrs == 0) {
    cout << endl;
    cout << "No RTL-SDR devices found." << endl;
    cout << "Exiting..." << endl;
    cout << endl;
    exit(0);
  } else {
    cout << "Found " << num_rtlsdrs << " RTL-SDR device" << ((num_rtlsdrs > 1) ? "s" : " ") << endl;
    cout << endl;
  }
	
  // identify the connected devices
  for (uint32_t i = 0; i < num_rtlsdrs; i++) {
		
    char maker[256];      // manufacturer
    char prodname[256];   // product name
    char sn[256];         // serial number
		
    rtlsdr_get_device_usb_strings(i, maker, prodname, sn);

    cout << "Device: " << i << endl;
    cout << "  Name: " << rtlsdr_get_device_name(i) << endl;
    cout << "  Manufacturer: " << maker << endl;
    cout << "  Product name: " << prodname << endl;
    cout << "  Serial number: " << sn << endl;
    cout << endl; 		
		
  } // end for (uint32_t i = 0; i < num_rtlsdrs; i++)
	
  // attempt to use device 0
  rtlsdr_dev_t *device;
	
  if (rtlsdr_open(&device, 0)) {
    cout << endl;
    cout << "Error opening RTL-SDR device 0!" << endl;
    cout << "Exiting..." << endl;
    cout << endl;
    exit(1);
  }
  
  cout << "Using device 0..." << endl;
  cout << endl;
  
  // check available gain settings
  
  int num_gain_values = rtlsdr_get_tuner_gains(device, NULL);   // get number of gain settings
  int *gain = new int [num_gain_values];
  rtlsdr_get_tuner_gains(device, gain);                         // store gain values in the array
  
  cout << "Available gain settings:" << endl;
  for (int i = 0; i < num_gain_values; i++) {
    cout << (gain[i]/10.0) << " ";
  }
  
  int gain_idx = num_gain_values - 1;   // index of current gain value

  cout << endl;
  rtlsdr_set_tuner_gain(device, gain[gain_idx]);
  cout << endl;
  cout << "Setting gain to " << (gain[gain_idx]/10.0) << "dB" << endl;
  cout << endl;
  
  cout << "Estimating oscillator frequency error using a known FM signal..." << endl;
  cout << endl;

  cout << "  Using the FM signal at " << (FM_CHANNEL * 1.0e-6) << "MHz" << endl; 
  rtlsdr_set_center_freq(device, FM_CHANNEL);   // set center frequncy
  cout << "    The RTL-SDR device reports the center frequency to be " << (rtlsdr_get_center_freq(device) * 1.0e-6) << "MHz" << endl;
  
  cout << "  Setting the sampling rate to " << (FS_FM * 1.0e-3) << "kHz..." << endl;
  rtlsdr_set_sample_rate(device, static_cast<uint32_t>(FS_FM));
  cout << "    The RTL-SDR device reports the sampling rate to be " << (rtlsdr_get_sample_rate(device) * 1.0e-3) << "kHz" << endl;
  
  rtlsdr_reset_buffer(device);
  
  const uint32_t BUFFSIZE = 2 * FFTSIZE;        // number of bytes in buffer
  uint8_t *fmbuff = new uint8_t [BUFFSIZE];     // dynamically allocate memory for FFT
  int bytes_read;                               // number of bytes read
  
  // N.B.: This section is not required for RTL-SDRs with TCXOs
  // c.f. https://www.rtl-sdr.com/tag/frequency-drift/

  cout << endl;
  cout << "  Waiting for " << WAIT_SEC << " seconds to allow oscillator to stabilize..." << endl;
  
  for (int i = WAIT_SEC; i >= 0; i--) {
    
    cout << "  " << i << "    " << flush;
    
    // perform reads to "energize" the RTL-SDR device
    // each iteration consumes around 34ms (30 iterations => 1 second)
    for (int j = 0; j < 30; j++) {
      
      rtlsdr_read_sync(device, fmbuff, BUFFSIZE, &bytes_read);     // blocking read. Attempt to get BUFFSIZE bytes from the RTL-SDR device
      if (bytes_read != BUFFSIZE) {
        cout << endl;
        cout << endl;
        cout << "At i = " << i << ": Expected to receive " << BUFFSIZE << " bytes but received " << bytes_read << "." << endl;
        cout << "Exiting..." << endl;
        cout << endl;
        rtlsdr_close(device);
        exit(1);
      }
    } // end for (int j = 0; j < 30; j++)

    cout << "\r" << flush;
    
  } // end for (int i = WAIT_SEC; i >= 0; i--)
  
  cout << "                     " << endl;

  // Check for saturation. Adjust gain as necessary...
  
  cout << "  Checking for saturation..." << endl;
  
  int8_t *ibuff = new int8_t [FFTSIZE];
  int8_t *qbuff = new int8_t [FFTSIZE];
  
  for (int i = 0; i < num_gain_values; i++) {
    
    rtlsdr_reset_buffer(device);
    rtlsdr_read_sync(device, fmbuff, BUFFSIZE, &bytes_read);
    
    if (bytes_read != BUFFSIZE) {
      cout << endl;
      cout << endl;
      cout << "At i = " << i << ": Expected to receive " << BUFFSIZE << " bytes but received " << bytes_read << "." << endl;
      cout << "Exiting..." << endl;
      cout << endl;
      rtlsdr_close(device);
      exit(1);
    }
    
    int isat_ctr = 0;
    int qsat_ctr = 0;
    int sat_limit = static_cast<int>(SAT_PERCENTAGE_LIMIT * FFTSIZE/100.0);
    
    // split received data into I and Q channels
    for (uint32_t j = 0, idx = 0; j < BUFFSIZE; j += 2, idx++) {
      
      // RTL-SDR output is offset binary
      // convert from offset binary to twos complement
      ibuff[idx] = fmbuff[j] - 128;                
      qbuff[idx] = fmbuff[j + 1] - 128;
      
      if ((ibuff[idx] == 127) || (ibuff[idx] == -128)) {
        ++isat_ctr;
      }
      if ((qbuff[idx] == 127) || (qbuff[idx] == -128)) {
        ++qsat_ctr;
      }
      
    } // end for (int j = 0; j < BUFFSIZE; j += 2) 
    
    if ((isat_ctr < sat_limit) && (qsat_ctr < sat_limit)) {
      continue;
    } else {
      --gain_idx;
      cout << "    Reducing gain to " << (gain[gain_idx]/10.0) << "dB" << endl;
      rtlsdr_set_tuner_gain(device, gain[gain_idx]);
    }
    
  } // end for (int i = 0; i < num_gain_values; i++)
  
  // release memory
  
  delete[] ibuff;
  delete[] qbuff;
  
  /*
  
  1. When no audio is being broadcast on FM, only the carrier and some sidebands are transmitted.
  2. Do get the frequency offset, we perform a "max-hold" on the power spectrum. Ideally, we catch
     a frame when no audio is being broadcast.
  3. Collecting 8192 I/Q samples at fs = 240kHz takes 8192/240kHz = 34.13ms
  4. If we take 10sec for the max-hold operation, we require 10/34.13ms = 293 frames (1 frame = 8192 I/Q samples)
  5. Let us then take the max-hold for >= 300 frames (10.24sec)
    
  */

  cout << endl;
  cout << "  Collecting data for " << FERR_FRAMES << " FFT frames..." << endl;
   
  fftwf_complex fft_in[FFTSIZE], fft_out[FFTSIZE];
  fftwf_complex ifft_in[FFTSIZE], ifft_out[FFTSIZE];
  fftwf_plan pfward = fftwf_plan_dft_1d(FFTSIZE, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
  fftwf_plan pbward = fftwf_plan_dft_1d(FFTSIZE, ifft_in, ifft_out, FFTW_BACKWARD, FFTW_ESTIMATE);
  
  float maxhold[FFTSIZE];

  const float FM_BIN = 1.0/FFTSIZE;     // frequency bin
  const float DF_FM = FS_FM * FM_BIN;   // frequency resolution of the estimator
  
  float foffset;                        // estimated frequency offset
  int maxloc;                           // location of highest peak
  float hpeak = 0.0;                    // value of highest peak
  float ppm_error;                      // frequency error in ppm

  // initialize maxhold
  for (uint32_t i = 0; i < FFTSIZE; i++) {
    maxhold[i] = 0.0f;
  } // end for (uint32_t i = 0; i < FFTSIZE; i++) 
    
  for (int i = 0; i < FERR_FRAMES; i++) {
    
    cout << "  " << (i + 1) << "    " << flush;
    
    rtlsdr_reset_buffer(device);
    rtlsdr_read_sync(device, fmbuff, BUFFSIZE, &bytes_read);
    if (bytes_read != BUFFSIZE) {
      cout << endl;
      cout << endl;
      cout << "At i = " << i << ": Expected to receive " << BUFFSIZE << " bytes but received " << bytes_read << "." << endl;
      cout << "Exiting..." << endl;
      cout << endl;
      rtlsdr_close(device);
      exit(1);
    }
    
    const float K = 1.0/128;            // scaling factor for integer to floating point conversion

    float idc = 0.0;
    float qdc = 0.0;
    
    for (uint32_t j = 0, idx = 0; j < BUFFSIZE; j += 2, idx++) {
      
      // convert from offset binary to floating point (-1.0 <= x < +1.0)
      Real(fft_in[idx]) = K * (fmbuff[j] - 128);
      Imag(fft_in[idx]) = K * (fmbuff[j + 1] - 128);
      
      // calculate DC offset
      idc += FM_BIN * Real(fft_in[idx]);
      qdc += FM_BIN * Imag(fft_in[idx]);
      
    } // end for (int j = 0, int idx = 0; j < BUFFSIZE; j += 2, idx++) 
    
    // remove DC offset
    for (uint32_t j = 0; j < FFTSIZE; j++) {
      Real(fft_in[j]) -= idc;
      Imag(fft_in[j]) -= qdc;
    } // end for (int j = 0; j < FFTSIZE; j++)
    
    fftwf_execute(pfward);  // perform FFT
    
    // calculate magnitude and update maxhold
    for (uint32_t j = 0; j < FFTSIZE; j++) {
      float mag = Square(Real(fft_out[j])) + Square(Imag(fft_out[j]));  // mag = i^2 + q^2
      if (mag > maxhold[j]) {
        maxhold[j] = mag;
      }
    } // end for (int j = 0; j < FFTSIZE; j++)
    
    cout << "\r" << flush;
        
  } // end for (int i = 0; i < FERR_FRAMES; i++)
  
  cout << "                " << endl;
  
  // calculate frequency offset
  
  hpeak = 0.0;
  for (uint32_t i = 0; i < FFTSIZE; i++) {
    if (maxhold[i] > hpeak) {
      hpeak = maxhold[i];
      maxloc = i;
    }
  } // end for (int i = 0; i < FFTSIZE; i++)
  
  cout << "  Highest peak = " << hpeak << " at index = " << maxloc << endl;
  
  if (maxloc < (static_cast<int>(FFTSIZE)/2)) {   // positive frequency half
    foffset = maxloc * DF_FM;
  } else {                      // negative frequency half
    foffset = (maxloc - static_cast<int>(FFTSIZE)) * DF_FM;
  } // end if-else (maxloc < (FFTSIZE/2))
  
  ppm_error = (foffset * 1.0e6)/FM_CHANNEL;
  
  cout << "  Estimated frequency offset is " << foffset << "Hz or " << ppm_error << "ppm" << endl;
  cout << endl;
  
  // apply frequency correction
  cout << "Adjusting for frequency offset..." << endl;
  rtlsdr_set_freq_correction(device, static_cast<int>(-ppm_error));
  /*
  // Perform C/A code acquisition
  
  cout << endl;
  cout << "C/A code acquisition" << endl;
  cout << endl;
  
  cout << "  Setting the LO frequency to " << (L1_CARRIER * 1.0e-6) << "MHz" << endl; 
  rtlsdr_set_center_freq(device, L1_CARRIER);   // set center frequncy
  cout << "    The RTL-SDR device reports the center frequency to be " << (rtlsdr_get_center_freq(device) * 1.0e-6) << "MHz" << endl;
  
  cout << "  Setting the sampling rate to " << (FSAMPLE * 1.0e-6) << "MHz..." << endl;
  rtlsdr_set_sample_rate(device, static_cast<uint32_t>(FSAMPLE));

  // set to maximum gain
  gain_idx = num_gain_values - 1;
  rtlsdr_set_tuner_gain(device, gain[gain_idx]);
  cout << "  Setting gain to " << (gain[gain_idx]/10.0) << "dB" << endl;
  
  // declarations for FFTs of satellite codes (SVN1 ~ 32)
  #include "ca_gen1_fft.hpp"
  #include "ca_gen2_fft.hpp"
  #include "ca_gen3_fft.hpp"
  #include "ca_gen4_fft.hpp"
  #include "ca_gen5_fft.hpp"
  #include "ca_gen6_fft.hpp"
  #include "ca_gen7_fft.hpp"
  #include "ca_gen8_fft.hpp"
  #include "ca_gen9_fft.hpp"
  #include "ca_gen10_fft.hpp"
  #include "ca_gen11_fft.hpp"
  #include "ca_gen12_fft.hpp"
  #include "ca_gen13_fft.hpp"
  #include "ca_gen14_fft.hpp"
  #include "ca_gen15_fft.hpp"
  #include "ca_gen16_fft.hpp"
  #include "ca_gen17_fft.hpp"
  #include "ca_gen18_fft.hpp"
  #include "ca_gen19_fft.hpp"
  #include "ca_gen20_fft.hpp"
  #include "ca_gen21_fft.hpp"
  #include "ca_gen22_fft.hpp"
  #include "ca_gen23_fft.hpp"
  #include "ca_gen24_fft.hpp"
  #include "ca_gen25_fft.hpp"
  #include "ca_gen26_fft.hpp"
  #include "ca_gen27_fft.hpp"
  #include "ca_gen28_fft.hpp"
  #include "ca_gen29_fft.hpp"
  #include "ca_gen30_fft.hpp"
  #include "ca_gen31_fft.hpp"
  #include "ca_gen32_fft.hpp"
  

  cout << "  Reading a block of data from RTL-SDR device..." << endl;
    
  rtlsdr_reset_buffer(device);
  rtlsdr_read_sync(device, fmbuff, BUFFSIZE, &bytes_read);
  if (bytes_read != BUFFSIZE) {
    cout << endl;
    cout << endl;
    cout << "At i = " << i << ": Expected to receive " << BUFFSIZE << " bytes but received " << bytes_read << "." << endl;
    cout << "Exiting..." << endl;
    cout << endl;
    rtlsdr_close(device);
    exit(1);
  }
  
  const float K = 1.0/128;            // scaling factor for integer to floating point conversion

  float idc = 0.0;
  float qdc = 0.0;    
  
  float *ibuff = new float [FFTSIZE];
  float *qbuff = new float [FFTSIZE];
  
  for (uint32_t j = 0, idx = 0; j < BUFFSIZE; j += 2, idx++) {
    
    // convert from offset binary to floating point (-1.0 <= x < +1.0)
    ibuff[idx] = K * (fmbuff[j    ] - 128);
    qbuff[idx] = K * (fmbuff[j + 1] - 128);
    
    // calculate DC offset
    idc += FM_BIN * ibuff[idx];
    qdc += FM_BIN * qbuff[idx];
    
  } // end for (int j = 0, int idx = 0; j < BUFFSIZE; j += 2, idx++) 
  
  // remove DC offset
  for (uint32_t j = 0; j < FFTSIZE; j++) {
    ibuff[j] -= idc;
    qbuff[j] -= qdc;
  } // end for (int j = 0; j < FFTSIZE; j++)
  
  
  
  // assume that the remaining frequency offset is within +/-10kHz
  const float FSTEP = static_cast<float>(FSAMPLE)/FFTSIZE;  // frequency sweep
  for (uint32_t i = 0; i < NUM_BINS; i++) {    
    
    float df;     // frequency offset
    if (i == 0) {
      df = 0.0;
    } else if (i % 2 == 1) {  // i is odd
      df = (-((i >> 1) + 1)) * FSTEP;
    } else {                  // i is even
      df = (i >> 1) * FSTEP;
    } // end if-else (i == 0)
    
    
    for (uint32_t j = 0; j < 32; j++) {     // satellite sweep
    } // end for (uint32_t j = 0; j < 32; j++)
    
  } // end for (uint32_t i = 0; i < FBINS; i++)
  
  */
  
  
  
  rtlsdr_close(device);
  
  cout << endl;   
  cout << endl;
  cout << "***********************************************************************" << endl;
  cout << "*                                                                     *" << endl;
  cout << "* m(_ _)m Thank you for using the Arty Z7 board with RTL-SDR! m(_ _)m *" << endl;
  cout << "*                                                                     *" << endl;
  cout << "***********************************************************************" << endl;
  cout << endl;
  cout << endl;
  
  return(0);
	
} // end main()
