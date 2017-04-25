// -*- c++ -*-
//  Class FCATAnalysis
//
//  Bryan Moffit, October 2012
//  Analyzing data from the Full Crate Acceptance Test
//

#define MAXCHAN 16
#define NROCS   1
#define MAXBLOCKLEVEL 1
#define FADC_MAX_MODULES 21
#define FADC_MAX_PULSES   2
#define FADC_MAX_CHAN    16
#define FADC_MAX_SAMPLES 100
#define ROC_DATA_HEADER          0xCEBAF000

#define CTP_MAX_SAMPLES 600

/* Define ADC Data Types and Masks */
/* #define FA_DAQ_HEADER             0xFADC0000 */
#define FA_DAQ_HEADER             0xF800FAFA

#define FA_DATA_TYPE_DEFINE       0x80000000
#define FA_DATA_TYPE_MASK         0x78000000
#define FA_DATA_SLOT_MASK         0x07c00000

#define FA_DATA_BLOCK_HEADER      0x00000000
#define FA_DATA_BLOCK_TRAILER     0x08000000
#define FA_DATA_EVENT_HEADER      0x10000000
#define FA_DATA_TRIGGER_TIME      0x18000000
#define FA_DATA_WINDOW_RAW        0x20000000
#define FA_DATA_WINDOW_SUM        0x28000000
#define FA_DATA_PULSE_RAW         0x30000000
#define FA_DATA_PULSE_INTEGRAL    0x38000000
#define FA_DATA_PULSE_TIME        0x40000000
#define FA_DATA_STREAM            0x48000000
#define FA_DATA_INVALID           0x70000000
#define FA_DATA_FILLER            0x78000000
#define FA_DUMMY_DATA             0xf800fafa

// #define FA_DATA_BLKNUM_MASK       0x0000003f
// #define FA_DATA_BLKLVL_MASK       0x003FF800
#define FA_DATA_BLKLVL_MASK       0x000000FF
#define FA_DATA_BLKNUM_MASK       0x0003FF00
#define FA_DATA_WRDCNT_MASK       0x003fffff
#define FA_DATA_TRIGNUM_MASK      0x07ffffff

#define FA_DATA_TRIGTIME_MASK     0x00FFFFFF
#define FA_DATA_CHAN_MASK         0x07800000
#define FA_DATA_WINWIDTH_MASK     0x00000FFF

#define FA_DATA_PULSE_NUMBER_MASK   0x00600000
#define FA_DATA_QUALITY_FACTOR_MASK 0x00180000
#define FA_DATA_PULSE_INTEGRAL_MASK 0x0007ffff
#define FA_DATA_PULSE_TIME_MASK     0x0000ffff

/* Define F1 stuff */
#define F1TDC_MAX_MODULES  FADC_MAX_MODULES
#define F1TDC_MAX_HITS     8
#define F1TDC_MAX_CHAN    48
#define F1TDC_MAX_CHIPS    8

#define F1_DATA_CHIP_TRIGGER_TIME_MASK 0x0000FF80
#define F1_DATA_CHIP_ADDR_MASK         0x00000038
#define F1_DATA_CHIP_ERROR_MASK        0x07000000
#define F1_DATA_NO_ERROR               0x4
#define F1_DATA_HIT_TIME_MASK          0x0000FFFF
#define F1_DATA_HIT_CHAN_ADDR_MASK     0x00070000
#define F1_DATA_HIT_CHIP_ADDR_MASK     0x00380000

#define F1_DATA_TRIGTIME1_MASK     0x00FFFFFF
#define F1_DATA_TRIGTIME2_MASK     0x00FFFFFF

#include <iostream>
#include <iomanip>
#include <vector>

#include "evioUtil.hxx"
#include "TROOT.h"
#include "TFile.h"
#include "TProfile.h"
#include "TTree.h"
#include "TRandom.h"
#include "TClonesArray.h"

using namespace std;
using namespace evio;



struct fadc_data_struct
{
  unsigned int new_type;
  unsigned int type;
  unsigned int slot_id_hd;
  unsigned int slot_id_tr;
  unsigned int n_evts;
  unsigned int blk_num;
  unsigned int n_words;
  unsigned int evt_num_1;
  unsigned int evt_num_2;
  unsigned int time_now;
  unsigned int time_1;
  unsigned int time_2;
  unsigned int time_3;
  unsigned int time_4;
  unsigned int chan;
  unsigned int width;
  unsigned int valid_1;
  unsigned int adc_1;
  unsigned int valid_2;
  unsigned int adc_2;
  unsigned int over;
  unsigned int adc_sum;
  unsigned int pulse_num;
  unsigned int thres_bin;
  unsigned int quality;
  unsigned int integral;
  unsigned int time;
  unsigned int chan_a;
  unsigned int source_a;
  unsigned int chan_b;
  unsigned int source_b;
  unsigned int group;
  unsigned int time_coarse;
  unsigned int time_fine;
  unsigned int vmin;
  unsigned int vpeak;
  unsigned int trig_type_int;	/* next 4 for internal trigger data */
  unsigned int trig_state_int;	/* e.g. helicity */
  unsigned int evt_num_int;
  unsigned int err_status_int;
};

struct moduleBank
{
  uint32_t length;
  uint32_t header;
  uint32_t data[];
};

struct rocBank
{
  UInt_t length;
  UInt_t header;
  UInt_t *data;
};

class roc_data:public TObject
{
public:
  UInt_t readoutTime;
  UInt_t bufAvail;
  UInt_t bufBusy;
  UInt_t eventLength;
  UInt_t datapos;

    roc_data()
  {
  }
  void Clear()
  {
    readoutTime = 0;
    bufAvail = 0;
    bufBusy = 0;
    eventLength = 0;
    datapos = 0;
  }

ClassDef(roc_data, 2)};

class tid_data:public TObject
{
public:
  UInt_t eventHeader;
  UInt_t eventNumber;
  UInt_t prev_eventNumber;
  Long64_t prev_timestamp;
  Long64_t timestamp;
  UInt_t triggerData;
  UInt_t extraData1;
  UInt_t extraData2;
  UInt_t tofb;
  UInt_t rollover;

    tid_data()
  {
    rollover = 0;
  }
  void Clear()
  {
    eventHeader = 0;
    eventNumber = 0;
    triggerData = 0;
    extraData1 = 0;
    extraData2 = 0;
    tofb = 0;
  }

ClassDef(tid_data, 2)};

class fadc_channel_data:public TObject
{
public:
  Int_t chan;
  // Pulse mode data
  Int_t nPulses;		// Number of pulses
  Int_t *pulse_integral;	//[nPulses]
  Int_t *pulse_time;		//[nPulses]
  Int_t ped_sum;
  //   Int_t pulse_integral[FADC_MAX_PULSES];
//   Int_t pulse_time[FADC_MAX_PULSES];
  // Raw window data
  Int_t nsamples;
  Int_t *raw_data;		//[nsamples]
  Int_t *sample;		//[nsamples]

    fadc_channel_data()
  {
    chan = -1;
    nPulses = 0;
    pulse_integral = 0;
    pulse_time = 0;
    raw_data = 0;
    sample = 0;
    nsamples = 0;
  }

  void Clear()
  {
    nPulses = 0;
    nsamples = 0;
    for (Int_t i = 0; i < FADC_MAX_PULSES; i++)
      {
	pulse_integral[i] = 0;
	pulse_time[i] = 0;
      }
    for (Int_t i = 0; i < FADC_MAX_SAMPLES; i++)
      {
	raw_data[i] = 0;
	sample[i] = 0;
      }
  }

ClassDef(fadc_channel_data, 2)};

class f1tdc_channel_data:public TObject
{
public:
  Int_t chan;
  Int_t nHits;			// Number of hits
  Int_t *time;			//[nHits]
  Int_t *trigtime;		//[nHits]
  Int_t *ctime;			//[nHits]

    f1tdc_channel_data()
  {
    chan = -1;
    nHits = 0;
    time = 0;
    trigtime = 0;
    ctime = 0;
  }
  void Clear()
  {
    nHits = 0;
    for (Int_t i = 0; i < F1TDC_MAX_HITS; i++)
      {
	time[i] = 0;
	trigtime[i] = 0;
	ctime[i] = 0;
      }
  }

ClassDef(f1tdc_channel_data, 2)};

class ctp_data:public TObject
{
public:
  Int_t nsamples;
  Int_t *data;			//[nsamples]
  Int_t *sample;		//[nsamples]

    ctp_data()
  {
    nsamples = 0;
    data = 0;
    sample = 0;
    data = new Int_t[CTP_MAX_SAMPLES];
    sample = new Int_t[CTP_MAX_SAMPLES];
  }

  void Clear()
  {
    nsamples = 0;
    for (Int_t isamp = 0; isamp < CTP_MAX_SAMPLES; isamp++)
      {
	data[isamp] = 0;
	sample[isamp] = 0;
      }
  }

ClassDef(ctp_data, 2)};

class fadc_block_data:public TObject
{
public:
  Int_t slotID;			// Module Slot ID
  UInt_t blockHeader;
  UInt_t blockTrailer;
  Long64_t triggerTime;

    fadc_block_data()
  {
  }
  void Clear()
  {
    slotID = 0;
    blockHeader = 0;
    blockTrailer = 0;
    triggerTime = 0;
  }

ClassDef(fadc_block_data, 2)};

class f1tdc_block_data:public TObject
{
public:
  Int_t slotID;
  UInt_t blockHeader;
  UInt_t blockTrailer;
  Long64_t triggerTime;

    f1tdc_block_data()
  {
  }
  void Clear()
  {
    slotID = 0;
    blockHeader = 0;
    blockTrailer = 0;
    triggerTime = 0;
  }

ClassDef(f1tdc_block_data, 2)};

class tid_block_data:public TObject
{
public:
  UInt_t blockHeader1;
  UInt_t blockHeader2;
  UInt_t blockTrailer;

    tid_block_data()
  {
  }
  void Clear()
  {
    blockHeader1 = 0;
    blockHeader2 = 0;
    blockTrailer = 0;
  }


ClassDef(tid_block_data, 2)};

class fadc_module:public TObject
{
public:
  Int_t slot;
  vector < fadc_channel_data > fchan;

  fadc_module()
  {
    slot = 0;
    fchan.resize(FADC_MAX_CHAN);
  }

ClassDef(fadc_module, 2)};

class f1tdc_module:public TObject
{
public:
  Int_t slot;
  vector < f1tdc_channel_data > fchan;

  f1tdc_module()
  {
    slot = 0;
    fchan.resize(F1TDC_MAX_CHAN);
  }

ClassDef(f1tdc_module, 2)};

class Trigger:public TObject
{
public:
  // TID Data
  tid_data fTID[NROCS];
  // FADC Data
//   vector <vector < vector <fadc_channel_data> > >  fFADC;//!
  vector < fadc_module > fmod;
  vector < f1tdc_module > f1mod;
  ctp_data fCTP;
  Long64_t trigtime[NROCS][FADC_MAX_MODULES];
  UInt_t trignum[NROCS][FADC_MAX_MODULES];
  Int_t fNhits[NROCS][FADC_MAX_MODULES];
  UInt_t hitchannel[NROCS][FADC_MAX_MODULES][FADC_MAX_CHAN * FADC_MAX_PULSES];
  UInt_t hitmod[NROCS][FADC_MAX_MODULES];
  Int_t ff1Nhits[NROCS][F1TDC_MAX_MODULES];
  UInt_t f1hitchannel[NROCS][F1TDC_MAX_MODULES][F1TDC_MAX_CHAN *
						F1TDC_MAX_HITS];
  UInt_t f1hitmod[NROCS][F1TDC_MAX_MODULES];
  UInt_t blockevent;
  UInt_t f1ErrCode[NROCS][F1TDC_MAX_MODULES][F1TDC_MAX_CHIPS];

    Trigger()
  {
//     fFADC.resize(NROCS);
    for (Int_t iroc = 0; iroc < NROCS; iroc++)
      {
	fmod.resize(FADC_MAX_MODULES);
	f1mod.resize(F1TDC_MAX_MODULES);
//      fFADC[iroc].resize(FADC_MAX_MODULES);
//      for(Int_t imod=0; imod<FADC_MAX_MODULES; imod++)
//        {
//          fFADC[iroc][imod].resize(FADC_MAX_CHAN);
//        }
      }
    for (Int_t iroc = 0; iroc < NROCS; iroc++)
      {
	for (Int_t imod = 0; imod < FADC_MAX_MODULES; imod++)
	  {
	    fNhits[iroc][imod] = 0;
	  }
	for (Int_t imod = 0; imod < F1TDC_MAX_MODULES; imod++)
	  {
	    ff1Nhits[iroc][imod] = 0;
	  }
      }
  }
  void Clear()
  {
    fCTP.Clear();
    for (Int_t iroc = 0; iroc < NROCS; iroc++)
      {
	fTID[iroc].Clear();
	for (Int_t imod = 0; imod < FADC_MAX_MODULES; imod++)
	  {
	    trigtime[iroc][imod] = 0;
	    trignum[iroc][imod] = 0;
	    for (Int_t ichan = 0; ichan < FADC_MAX_CHAN; ichan++)
	      {
//              fFADC[iroc][imod][ichan].Clear();
		fmod[imod].fchan[ichan].Clear();
	      }
	    for (Int_t ichan = 0; ichan < F1TDC_MAX_CHAN; ichan++)
	      {
		f1mod[imod].fchan[ichan].Clear();
	      }

	    fNhits[iroc][imod] = 0;
	    hitmod[iroc][imod] = 0;
	    f1hitmod[iroc][imod] = 0;

	    for (Int_t i = 0; i < FADC_MAX_CHAN * FADC_MAX_PULSES; i++)
	      {
		hitchannel[iroc][imod][i] = 0;
	      }
	    for (Int_t i = 0; i < F1TDC_MAX_CHAN * F1TDC_MAX_HITS; i++)
	      {
		f1hitchannel[iroc][imod][i] = 0;
	      }
	    for (Int_t i = 0; i < F1TDC_MAX_CHIPS; i++)
	      {
		f1ErrCode[iroc][imod][i] = 0;
	      }
	  }
      }
  }

  ClassDef(Trigger, 2);
};

class stats:public TObject
{
  // Here's a fine place to keep statistics that accumulate or stay the
  // same through the entire run.
public:
  UInt_t PayloadType;
  UInt_t PayloadChannelMask[FADC_MAX_MODULES];
  UInt_t PayloadIntegral;

  UInt_t Latency; 
  UInt_t Window;

  UInt_t nMod;
  UInt_t ScanMask;
  TString SerialNumber[FADC_MAX_MODULES];
  UInt_t FirmwareVersion[FADC_MAX_MODULES];

  UInt_t nEntries;
  
  stats()
  {
    ScanMask = 0;
    Latency = 0;
    Window = 0;
    nMod = 0;
    nEntries = 0;
    for (Int_t imod = 0; imod < FADC_MAX_MODULES; imod++)
      {
	SerialNumber[imod] = "none";
	PayloadChannelMask[imod] = 0;
	FirmwareVersion[imod] = 0;
      }
    PayloadType = 0;
    PayloadIntegral = 0;
  }

ClassDef(stats, 2)};

class FCATAnalysis
{

public:

  FCATAnalysis(int);
   ~FCATAnalysis();
  void Init(const int *buffer, int skip);	//!
  void Process(const int *buffer);	//!
  void End();
  void faDataDecode(unsigned int data);
  void f1DataDecode(unsigned int data);
private:

    FCATAnalysis(const FCATAnalysis & fn);
    FCATAnalysis & operator=(const FCATAnalysis & fn);
  void Load(const uint32_t * buffer);
  void Decode();
  int DecodeTI(int iroc, UInt_t blocklevel, struct moduleBank *tibank,
	       unsigned int rocnumber);
  int DecodeFADC(int iroc, UInt_t blocklevel, struct moduleBank *fabank,
		 unsigned int rocnumber);
  int DecodeF1TDC(int iroc, UInt_t blocklevel, struct moduleBank *f1bank,
		  unsigned int rocnumber);
  int DecodeCTP(int iroc, UInt_t blocklevel, struct moduleBank *ctpbank,
		unsigned int rocnumber);
  void PrintBuffer(const uint32_t * buffer);	// print data buffer for diagnostics
  void PrintData();		// print data (sorted)
  Bool_t IsPrestartEvent() const;
  Bool_t IsPhysicsEvent() const;
  Bool_t IsSetupEvent() const;
  Int_t rocIndexFromNumber(int rocNumber);
  UInt_t DecodeSetup(struct moduleBank *setupbank);
  UInt_t DecodeF1Chan(UInt_t type, UInt_t chanaddr, UInt_t chipaddr);

  // Bank Info
  struct rocBank *rBank;
  struct moduleBank *mBank[10];

  UInt_t rocnames[NROCS];
  Int_t debug;
  Int_t fCodaEvTag;
  Int_t fBlocklevel;
  Int_t fRunNumber;
  Int_t fEventNumber;
  Int_t fEventType;
  Int_t fTimeStamp;
  Int_t fEvLength;
  Int_t fPrevEvLength;
  UInt_t bankmask;
  Bool_t skipFADC;
  Bool_t skipBlock;

  tid_block_data *TID;
  roc_data *ROC;
  fadc_block_data *FADC;
  f1tdc_block_data *F1TDC;

  TFile *evFile;		// ROOT output file
  TTree *evTree;		// Event Tree
  TTree *bTree;			// Event Tree
  Int_t initialized;
  UInt_t fEvType;
  UInt_t prev_eventNumber[NROCS];
  Long64_t prev_timestamp[NROCS];

  TH1I *trigger_period;

  TH1I *mod_hit[FADC_MAX_MODULES];
  TH1I *channel_hit[FADC_MAX_MODULES][FADC_MAX_CHAN];

  TH1I *timestamp_diff[FADC_MAX_MODULES];
  TH1I *raw_data[FADC_MAX_MODULES][FADC_MAX_CHAN];
  TH1I *pulse_time[FADC_MAX_MODULES][FADC_MAX_CHAN];
  TH1I *pulse_integral[FADC_MAX_MODULES][FADC_MAX_CHAN];
  
  Trigger currentTrigger;	// current trigger to write into tree
  Trigger *aTrigger;		// Array of triggers from current event, size = blocklevel

  stats *runstats;

};
